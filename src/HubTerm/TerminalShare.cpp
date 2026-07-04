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

#include "TerminalShare.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

#include "Pty/Pty.h"

TerminalShare::TerminalShare(Pty *pty, QWebSocket *ws, const QString &sessionId,
							 const QString &portName, QObject *parent /*= nullptr*/)
	: QObject(parent)
	, m_pty(pty)
	, m_ws(ws)
	, m_sessionId(sessionId)
	, m_portName(portName)
	, m_connectedAt(QDateTime::currentSecsSinceEpoch())
	, m_writable(true)
	, m_readonly(false)
{
	if (m_pty) {
		connect(m_pty, &Pty::readyRead, this, &TerminalShare::onPtyReadyRead);
		connect(m_pty, &QObject::destroyed, this, &TerminalShare::onPtyDestroyed);
	}
}

TerminalShare::~TerminalShare() {
	if (m_pty) {
		disconnect(m_pty, nullptr, this, nullptr);
	}
}

void TerminalShare::setWebSocket(QWebSocket *ws) {
	m_ws = ws;
}

void TerminalShare::setWritable(bool writable) {
	m_writable = writable;
}

bool TerminalShare::isWritable() const {
	return m_writable;
}

void TerminalShare::setReadonly(bool readonly) {
	m_readonly = readonly;
}

bool TerminalShare::isReadonly() const {
	return m_readonly;
}

void TerminalShare::onPtyReadyRead() {
	if (m_pty == nullptr || m_ws == nullptr) {
		return;
	}

	QByteArray data = m_pty->readAll();

	if (data.isEmpty()) {
		return;
	}

	QJsonObject msg;
	msg[QStringLiteral("type")] = QStringLiteral("terminal_data");
	QJsonObject payload;
	payload[QStringLiteral("session_id")] = m_sessionId;
	payload[QStringLiteral("direction")] = QStringLiteral("output");
	payload[QStringLiteral("data")] = QString::fromLatin1(data.toBase64());
	msg[QStringLiteral("data")] = payload;

	QJsonDocument doc(msg);
	m_ws->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

void TerminalShare::onRemoteWrite(const QByteArray &data) {
	if (m_pty == nullptr || m_readonly) {
		return;
	}

	m_pty->write(data);
}

void TerminalShare::onPtyDestroyed() {
	m_pty = nullptr;
}
