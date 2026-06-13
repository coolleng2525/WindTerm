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

#include <QJsonDocument>
#include <QJsonObject>
#include <QWebSocket>

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
{}

void HubTermAgent::start(const QString &centerUrl) {
	if (m_ws != nullptr && m_ws->state() != QAbstractSocket::UnconnectedState) {
		return;
	}

	m_centerUrl = centerUrl;
	m_stopping = false;
	m_reconnectDelay = 1000;

	if (m_ws == nullptr) {
		m_ws = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

		connect(m_ws, &QWebSocket::connected, this, &HubTermAgent::onWsConnected);
		connect(m_ws, &QWebSocket::disconnected, this, &HubTermAgent::onWsDisconnected);
		connect(m_ws, &QWebSocket::textMessageReceived, this, &HubTermAgent::onWsTextMessage);
		connect(m_ws, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
				this, &HubTermAgent::onWsError);
	}

	m_ws->open(QUrl(m_centerUrl));
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
}

bool HubTermAgent::isConnected() const {
	return m_ws != nullptr && m_ws->state() == QAbstractSocket::ConnectedState;
}

void HubTermAgent::attachPty(Pty *pty) {
	if (pty && !m_attachedPtys.contains(pty)) {
		m_attachedPtys.append(pty);
	}
}

void HubTermAgent::detachPty(Pty *pty) {
	m_attachedPtys.removeAll(pty);
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

	QJsonObject cmd = doc.object();
	emit commandReceived(cmd);
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
				m_ws->open(QUrl(m_centerUrl));
			}
		});
	}

	m_reconnectTimer->start(m_reconnectDelay);

	m_reconnectDelay = qMin(m_reconnectDelay * 2, 30000);
}

void HubTermAgent::sendReport() {
	if (!isConnected()) {
		return;
	}

	HubTermReporter reporter;
	QJsonObject report;

	report[QStringLiteral("type")] = QStringLiteral("report");
	report[QStringLiteral("node_id")] = HubTermConfig::instance()->nodeId();
	report[QStringLiteral("node_name")] = HubTermConfig::instance()->nodeName();
	report[QStringLiteral("domain")] = HubTermConfig::instance()->domain();
	report[QStringLiteral("system_info")] = reporter.collectSystemInfo();
	report[QStringLiteral("capabilities")] = reporter.collectCapabilities();
	report[QStringLiteral("serial_ports")] = reporter.collectSerialPorts();
	report[QStringLiteral("attached_terminals")] = static_cast<int>(m_attachedPtys.size());

	QJsonDocument doc(report);
	m_ws->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}
