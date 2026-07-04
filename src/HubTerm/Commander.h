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

#ifndef HUBTERM_COMMANDER_H
#define HUBTERM_COMMANDER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <functional>

class Pty;

class HubTermCommander
	: public QObject
{
	Q_OBJECT

public:
	enum class CommandType {
		Unknown,
		Connect,
		Disconnect,
		ExecScript,
		SetPermission,
		UpdateConfig,
		Restart,
		WriteToTerminal,
		GetStatus
	};

	explicit HubTermCommander(QObject *parent = nullptr);
	~HubTermCommander() = default;

	bool execute(const QJsonObject &command);
	CommandType parseCommandType(const QString &type) const;

	using CommandHandler = std::function<bool(const QJsonObject &)>;
	void registerHandler(CommandType type, CommandHandler handler);

signals:
	void commandReceived(const QJsonObject &command);
	void commandResult(const QJsonObject &result);

private:
	bool handleConnect(const QJsonObject &params);
	bool handleDisconnect(const QJsonObject &params);
	bool handleExecScript(const QJsonObject &params);
	bool handleSetPermission(const QJsonObject &params);
	bool handleUpdateConfig(const QJsonObject &params);
	bool handleRestart(const QJsonObject &params);
	bool handleWriteToTerminal(const QJsonObject &params);
	bool handleGetStatus(const QJsonObject &params);

	QJsonObject buildResponse(const QString &commandId, bool success,
							  const QString &message, const QJsonObject &data = QJsonObject()) const;

	QMap<CommandType, CommandHandler> m_handlers;
	QJsonObject m_lastResultData;
	QString m_lastResultMessage;
};

#endif // HUBTERM_COMMANDER_H
