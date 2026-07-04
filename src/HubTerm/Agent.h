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

#ifndef HUBTERM_AGENT_H
#define HUBTERM_AGENT_H

#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <QJsonObject>
#include <QList>
#include <QHash>

class Pty;
class TerminalShare;
class HubTermCommander;

class HubTermAgent
	: public QObject
{
	Q_OBJECT

public:
	static HubTermAgent *instance();

	void start(const QString &centerUrl);
	void stop();
	bool isConnected() const;

	void attachPty(Pty *pty);
	void detachPty(Pty *pty);

	QWebSocket *webSocket() const { return m_ws; }

signals:
	void connected();
	void disconnected();
	void commandReceived(const QJsonObject &cmd);

private:
	explicit HubTermAgent(QObject *parent = nullptr);
	~HubTermAgent() = default;

	void onWsConnected();
	void onWsDisconnected();
	void onWsTextMessage(const QString &message);
	void onWsError(QAbstractSocket::SocketError error);
	void tryReconnect();
	bool ensureRegistered();
	void openWebSocket();
	void sendReport();
	QJsonObject buildNodeReport() const;
	QString agentUrl() const;
	QString reportUrl() const;
	QString normalizeCenterUrl(const QString &centerUrl) const;
	void routeCommand(const QJsonObject &message);
	void handleWriteCommand(const QJsonObject &payload);

	QWebSocket *m_ws;
	QTimer *m_reconnectTimer;
	QTimer *m_reportTimer;
	QString m_centerUrl;
	int m_reconnectDelay;
	bool m_stopping;
	QList<Pty *> m_attachedPtys;
	QHash<Pty *, TerminalShare *> m_terminalShares;
	HubTermCommander *m_commander;

	static HubTermAgent *s_instance;
};

#endif // HUBTERM_AGENT_H
