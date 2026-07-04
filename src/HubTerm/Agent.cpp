/*
 * Copyright 2020, WindTerm.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Agent.h"
#include "Reporter.h"
#include "Config.h"
#include "Commander.h"
#include "TerminalShare.h"

#include <QAbstractSocket>
#include <QByteArray>
#include <QDateTime>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSysInfo>
#include <QUrl>
#include <QUrlQuery>
#include <QWebSocket>
#include <QUuid>

#include "Pty/Pty.h"

HubTermAgent *HubTermAgent::s_instance = nullptr;

HubTermAgent *HubTermAgent::instance() {
	if (s_instance == nullptr) {
		s_instance = new HubTermAgent();
	}
	return s_instance;
}

HubTermAgent::HubTermAgent(QObject *parent /*= nullptr*/)
	: QObject(parent)
	, m_ws(nullptr)
	, m_reconnectTimer(nullptr)
	, m_reportTimer(nullptr)
	, m_reconnectDelay(1000)
	, m_stopping(false)
	, m_commander(new HubTermCommander(this))
{
	connect(this, &HubTermAgent::commandReceived, m_commander, &HubTermCommander::execute);
	connect(m_commander, &HubTermCommander::commandResult, this, [this](const QJsonObject &result) {
		if (!isConnected()) {
			return;
		}
		QJsonDocument doc(result);
		m_ws->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
	});
}

void HubTermAgent::start(const QString &centerUrl) {
	if (m_ws != nullptr && m_ws->state() != QAbstractSocket::UnconnectedState) {
		return;
	}

	m_centerUrl = centerUrl;
	m_stopping = false;
	m_reconnectDelay = 1000;
	HubTermConfig::instance()->setCenterUrl(normalizeCenterUrl(centerUrl));

	if (m_ws == nullptr) {
		m_ws = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

		connect(m_ws, &QWebSocket::connected, this, &HubTermAgent::onWsConnected);
		connect(m_ws, &QWebSocket::disconnected, this, &HubTermAgent::onWsDisconnected);
		connect(m_ws, &QWebSocket::textMessageReceived, this, &HubTermAgent::onWsTextMessage);
		connect(m_ws, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
				this, &HubTermAgent::onWsError);
	}
	for (TerminalShare *share : m_terminalShares) {
		share->setWebSocket(m_ws);
	}

	if (!ensureRegistered()) {
		tryReconnect();
		return;
	}

	openWebSocket();
}

void HubTermAgent::stop() {
	m_stopping = true;

	if (m_reconnectTimer) {
		m_reconnectTimer->stop();
	}

	if (m_reportTimer) {
		m_reportTimer->stop();
	}

	if (m_ws) {
		m_ws->close();
		m_ws->deleteLater();
		m_ws = nullptr;
	}

	qDeleteAll(m_terminalShares);
	m_terminalShares.clear();
}

bool HubTermAgent::isConnected() const {
	return m_ws != nullptr && m_ws->state() == QAbstractSocket::ConnectedState;
}

void HubTermAgent::attachPty(Pty *pty) {
	if (pty && !m_attachedPtys.contains(pty)) {
		m_attachedPtys.append(pty);
		pty->setHubTermEnabled(true);

		const QString sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
		TerminalShare *share = new TerminalShare(pty, m_ws, sessionId, QStringLiteral("WindTerm terminal"), this);
		m_terminalShares.insert(pty, share);
		sendReport();
	}
}

void HubTermAgent::detachPty(Pty *pty) {
	m_attachedPtys.removeAll(pty);
	if (m_terminalShares.contains(pty)) {
		delete m_terminalShares.take(pty);
	}
	if (pty) {
		pty->setHubTermEnabled(false);
	}
	sendReport();
}

void HubTermAgent::onWsConnected() {
	m_reconnectDelay = 1000;

	if (m_reconnectTimer) {
		m_reconnectTimer->stop();
	}

	if (m_reportTimer == nullptr) {
		m_reportTimer = new QTimer(this);
		connect(m_reportTimer, &QTimer::timeout, this, &HubTermAgent::sendReport);
	}

	int interval = HubTermConfig::instance()->reportInterval();
	m_reportTimer->start(interval);

	emit connected();
}

void HubTermAgent::onWsDisconnected() {
	if (m_reportTimer) {
		m_reportTimer->stop();
	}

	emit disconnected();

	if (!m_stopping) {
		tryReconnect();
	}
}

void HubTermAgent::onWsTextMessage(const QString &message) {
	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);

	if (parseError.error != QJsonParseError::NoError) {
		return;
	}

	if (!doc.isObject()) {
		return;
	}

	routeCommand(doc.object());
}

void HubTermAgent::onWsError(QAbstractSocket::SocketError error) {
	Q_UNUSED(error)
}

void HubTermAgent::tryReconnect() {
	if (m_stopping || m_ws == nullptr) {
		return;
	}

	if (m_reconnectTimer == nullptr) {
		m_reconnectTimer = new QTimer(this);
		m_reconnectTimer->setSingleShot(true);
		connect(m_reconnectTimer, &QTimer::timeout, this, [this]() {
			if (!m_stopping && m_ws) {
				start(m_centerUrl);
			}
		});
	}

	m_reconnectTimer->start(m_reconnectDelay);

	m_reconnectDelay = qMin(m_reconnectDelay * 2, 30000);
}

bool HubTermAgent::ensureRegistered() {
	HubTermConfig *cfg = HubTermConfig::instance();
	if (cfg->nodeId().isEmpty()) {
		cfg->setNodeId(QUuid::createUuid().toString(QUuid::WithoutBraces));
	}
	if (!cfg->token().isEmpty()) {
		cfg->save();
		return true;
	}

	QNetworkRequest request{QUrl(reportUrl())};
	request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

	QNetworkAccessManager manager;
	QNetworkReply *reply = manager.post(request, QJsonDocument(buildNodeReport()).toJson(QJsonDocument::Compact));
	QEventLoop loop;
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	const bool ok = reply->error() == QNetworkReply::NoError;
	if (ok) {
		QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
		const QString token = doc.object().value(QStringLiteral("token")).toString();
		if (!token.isEmpty()) {
			cfg->setToken(token);
			cfg->save();
		}
	}
	reply->deleteLater();
	return ok && !cfg->token().isEmpty();
}

void HubTermAgent::openWebSocket() {
	QNetworkRequest request{QUrl(agentUrl())};
	const QString token = HubTermConfig::instance()->token();
	if (!token.isEmpty()) {
		request.setRawHeader("Sec-WebSocket-Protocol",
							 QStringLiteral("hubterm, hubterm.node.%1").arg(token).toUtf8());
	}
	m_ws->open(request);
}

void HubTermAgent::sendReport() {
	if (!isConnected()) {
		return;
	}

	QJsonObject msg;
	msg[QStringLiteral("type")] = QStringLiteral("report");
	msg[QStringLiteral("data")] = buildNodeReport();

	QJsonDocument doc(msg);
	m_ws->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

QJsonObject HubTermAgent::buildNodeReport() const {
	HubTermReporter reporter;
	QJsonObject report;

	report[QStringLiteral("node_id")] = HubTermConfig::instance()->nodeId();
	report[QStringLiteral("source")] = QStringLiteral("agent");
	report[QStringLiteral("name")] = HubTermConfig::instance()->nodeName();
	report[QStringLiteral("hostname")] = QSysInfo::machineHostName();
	report[QStringLiteral("os")] = reporter.osName();
	report[QStringLiteral("os_version")] = QSysInfo::prettyProductName();
	report[QStringLiteral("arch")] = reporter.cpuArchitecture();

	QJsonObject metrics = reporter.collectSystemMetrics();
	report[QStringLiteral("cpu_percent")] = metrics.value(QStringLiteral("cpu_percent")).toDouble();
	report[QStringLiteral("memory_total")] = metrics.value(QStringLiteral("memory_total")).toDouble();
	report[QStringLiteral("memory_used")] = metrics.value(QStringLiteral("memory_used")).toDouble();
	report[QStringLiteral("memory_percent")] = metrics.value(QStringLiteral("memory_percent")).toDouble();
	report[QStringLiteral("disk_total")] = metrics.value(QStringLiteral("disk_total")).toDouble();
	report[QStringLiteral("disk_used")] = metrics.value(QStringLiteral("disk_used")).toDouble();
	report[QStringLiteral("serial_ports")] = reporter.collectSerialPorts();

	QJsonArray sessions;
	for (TerminalShare *share : m_terminalShares) {
		QJsonObject session;
		session[QStringLiteral("session_id")] = share->sessionId();
		session[QStringLiteral("port_name")] = share->portName();
		session[QStringLiteral("user")] = QString();
		session[QStringLiteral("type")] = QStringLiteral("master");
		session[QStringLiteral("client_ip")] = QString();
		session[QStringLiteral("connected_at")] = share->connectedAt();
		sessions.append(session);
	}
	report[QStringLiteral("sessions")] = sessions;
	return report;
}

QString HubTermAgent::normalizeCenterUrl(const QString &centerUrl) const {
	QUrl url(centerUrl);
	if (!url.isValid()) {
		return centerUrl;
	}
	if (url.path().isEmpty() || url.path() == QLatin1String("/") || url.path() == QLatin1String("/ws") || url.path() == QLatin1String("/api/ws")) {
		url.setPath(QStringLiteral("/api/ws/agent"));
	}
	return url.toString();
}

QString HubTermAgent::agentUrl() const {
	QUrl url(HubTermConfig::instance()->centerUrl());
	url.setQuery(QString());
	QUrlQuery query;
	query.addQueryItem(QStringLiteral("node_id"), HubTermConfig::instance()->nodeId());
	url.setQuery(query);
	return url.toString();
}

QString HubTermAgent::reportUrl() const {
	QUrl url(HubTermConfig::instance()->centerUrl());
	url.setScheme(url.scheme() == QLatin1String("wss") ? QStringLiteral("https") : QStringLiteral("http"));
	url.setPath(QStringLiteral("/api/nodes/report"));
	url.setQuery(QString());
	return url.toString();
}

void HubTermAgent::routeCommand(const QJsonObject &message) {
	const QString type = message.value(QStringLiteral("type")).toString();
	const QJsonObject data = message.value(QStringLiteral("data")).toObject();
	const QJsonObject payload = data.value(QStringLiteral("payload")).toObject(data);

	if (type == QLatin1String("write")) {
		handleWriteCommand(payload);
		return;
	}
	if (type == QLatin1String("disconnect") || type == QLatin1String("kick_session")) {
		const QString sessionId = payload.value(QStringLiteral("session_id")).toString();
		for (auto it = m_terminalShares.begin(); it != m_terminalShares.end(); ++it) {
			if (it.value()->sessionId() == sessionId) {
				detachPty(it.key());
				return;
			}
		}
	}

	emit commandReceived(message);
}

void HubTermAgent::handleWriteCommand(const QJsonObject &payload) {
	const QString sessionId = payload.value(QStringLiteral("session_id")).toString();
	const QByteArray data = QByteArray::fromBase64(payload.value(QStringLiteral("data")).toString().toLatin1());

	for (TerminalShare *share : m_terminalShares) {
		if (share->sessionId() == sessionId) {
			share->onRemoteWrite(data);
			return;
		}
	}
}
