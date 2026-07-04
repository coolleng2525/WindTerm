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

#include "Commander.h"
#include "Config.h"

#include <QCoreApplication>
#include <QProcess>
#include <QTimer>

HubTermCommander::HubTermCommander(QObject *parent /*= nullptr*/)
	: QObject(parent)
{
	// Register default handlers
	m_handlers[CommandType::Connect] = [this](const QJsonObject &params) {
		return handleConnect(params);
	};
	m_handlers[CommandType::Disconnect] = [this](const QJsonObject &params) {
		return handleDisconnect(params);
	};
	m_handlers[CommandType::ExecScript] = [this](const QJsonObject &params) {
		return handleExecScript(params);
	};
	m_handlers[CommandType::SetPermission] = [this](const QJsonObject &params) {
		return handleSetPermission(params);
	};
	m_handlers[CommandType::UpdateConfig] = [this](const QJsonObject &params) {
		return handleUpdateConfig(params);
	};
	m_handlers[CommandType::Restart] = [this](const QJsonObject &params) {
		return handleRestart(params);
	};
	m_handlers[CommandType::WriteToTerminal] = [this](const QJsonObject &params) {
		return handleWriteToTerminal(params);
	};
	m_handlers[CommandType::GetStatus] = [this](const QJsonObject &params) {
		return handleGetStatus(params);
	};
}

HubTermCommander::CommandType HubTermCommander::parseCommandType(const QString &type) const {
	if (type == QLatin1String("connect")) return CommandType::Connect;
	if (type == QLatin1String("disconnect")) return CommandType::Disconnect;
	if (type == QLatin1String("exec") || type == QLatin1String("exec_script")) return CommandType::ExecScript;
	if (type == QLatin1String("set_permission")) return CommandType::SetPermission;
	if (type == QLatin1String("update_config")) return CommandType::UpdateConfig;
	if (type == QLatin1String("restart")) return CommandType::Restart;
	if (type == QLatin1String("write")) return CommandType::WriteToTerminal;
	if (type == QLatin1String("get_status")) return CommandType::GetStatus;
	return CommandType::Unknown;
}

bool HubTermCommander::execute(const QJsonObject &command) {
	emit commandReceived(command);
	m_lastResultData = QJsonObject();
	m_lastResultMessage = QString();

	QString typeStr = command.value(QStringLiteral("type")).toString();
	QJsonObject data = command.value(QStringLiteral("data")).toObject();
	QString commandId = data.value(QStringLiteral("id")).toString(command.value(QStringLiteral("id")).toString());
	QJsonObject params = data.value(QStringLiteral("payload")).toObject(command.value(QStringLiteral("params")).toObject());

	CommandType type = parseCommandType(typeStr);

	if (type == CommandType::Unknown) {
		QJsonObject result = buildResponse(commandId, false,
			QStringLiteral("Unknown command type: %1").arg(typeStr));
		emit commandResult(result);
		return false;
	}

	auto it = m_handlers.find(type);
	if (it == m_handlers.end()) {
		QJsonObject result = buildResponse(commandId, false,
			QStringLiteral("No handler registered for command type: %1").arg(typeStr));
		emit commandResult(result);
		return false;
	}

	bool success = it.value()(params);

	QJsonObject result = buildResponse(commandId, success,
		m_lastResultMessage.isEmpty()
			? (success ? QStringLiteral("Command executed successfully") : QStringLiteral("Command execution failed"))
			: m_lastResultMessage,
		m_lastResultData);
	emit commandResult(result);
	return success;
}

void HubTermCommander::registerHandler(CommandType type, CommandHandler handler) {
	m_handlers[type] = handler;
}

bool HubTermCommander::handleConnect(const QJsonObject &params) {
	Q_UNUSED(params)
	// Connect to a remote device via SSH/serial
	// This is a stub — the actual connection logic is in the closed-source GUI
	// The HubTerm agent emits a signal that the GUI can intercept
	m_lastResultMessage = QStringLiteral("Connect command received. GUI integration required for full implementation.");
	return true;
}

bool HubTermCommander::handleDisconnect(const QJsonObject &params) {
	Q_UNUSED(params)
	m_lastResultMessage = QStringLiteral("Disconnect command received.");
	return true;
}

bool HubTermCommander::handleExecScript(const QJsonObject &params) {
	QString script = params.value(QStringLiteral("command")).toString(params.value(QStringLiteral("script")).toString());
	QString shell = params.value(QStringLiteral("shell")).toString(
		QStringLiteral("/bin/sh"));

	if (script.isEmpty()) {
		return false;
	}

	QProcess process;
	process.start(shell, QStringList() << QStringLiteral("-c") << script);

	if (!process.waitForStarted(5000)) {
		return false;
	}

	if (!process.waitForFinished(30000)) {
		process.kill();
		return false;
	}

	QJsonObject data;
	data[QStringLiteral("stdout")] = QString::fromUtf8(process.readAllStandardOutput());
	data[QStringLiteral("stderr")] = QString::fromUtf8(process.readAllStandardError());
	data[QStringLiteral("exit_code")] = process.exitCode();

	m_lastResultMessage = QStringLiteral("Script executed");
	m_lastResultData = data;
	return true;
}

bool HubTermCommander::handleSetPermission(const QJsonObject &params) {
	Q_UNUSED(params)
	// Permission management — stub for now
	return true;
}

bool HubTermCommander::handleUpdateConfig(const QJsonObject &params) {
	QString centerUrl = params.value(QStringLiteral("center_url")).toString();
	QString nodeName = params.value(QStringLiteral("node_name")).toString();
	int reportInterval = params.value(QStringLiteral("report_interval")).toInt(0);

	HubTermConfig *cfg = HubTermConfig::instance();

	if (!centerUrl.isEmpty()) {
		cfg->setCenterUrl(centerUrl);
	}
	if (!nodeName.isEmpty()) {
		cfg->setNodeName(nodeName);
	}
	if (reportInterval > 0) {
		cfg->setReportInterval(reportInterval);
	}

	return cfg->save();
}

bool HubTermCommander::handleRestart(const QJsonObject &params) {
	Q_UNUSED(params)
	// Schedule a restart via timer to allow the response to be sent first
	QTimer::singleShot(1000, qApp, &QCoreApplication::quit);
	return true;
}

bool HubTermCommander::handleWriteToTerminal(const QJsonObject &params) {
	Q_UNUSED(params)
	// Terminal write is handled by TerminalShare via dataToSend signal
	return true;
}

bool HubTermCommander::handleGetStatus(const QJsonObject &params) {
	Q_UNUSED(params)
	QJsonObject data;

	data[QStringLiteral("connected")] = true;
	data[QStringLiteral("version")] = QStringLiteral("1.0.0");
	data[QStringLiteral("uptime")] = 0;

	m_lastResultMessage = QStringLiteral("Status retrieved");
	m_lastResultData = data;
	return true;
}

QJsonObject HubTermCommander::buildResponse(const QString &commandId, bool success,
											const QString &message, const QJsonObject &data) const {
	QJsonObject response;

	response[QStringLiteral("type")] = QStringLiteral("exec_result");

	QJsonObject payload;
	payload[QStringLiteral("cmd_id")] = commandId;
	payload[QStringLiteral("exit_code")] = success ? 0 : 1;
	if (success) {
		payload[QStringLiteral("stdout")] = message;
	} else {
		payload[QStringLiteral("stderr")] = message;
	}

	if (!data.isEmpty()) {
		for (auto it = data.begin(); it != data.end(); ++it) {
			payload[it.key()] = it.value();
		}
	}
	response[QStringLiteral("data")] = payload;

	return response;
}
