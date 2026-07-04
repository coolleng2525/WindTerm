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

#include "Config.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QSysInfo>

static const char *CONFIG_FILE_NAME = "hubterm.json";
static const int DEFAULT_REPORT_INTERVAL = 3000; // 3 seconds

HubTermConfig *HubTermConfig::instance() {
	static HubTermConfig *inst = new HubTermConfig();
	return inst;
}

HubTermConfig::HubTermConfig(QObject *parent /*= nullptr*/)
	: QObject(parent)
	, m_reportInterval(DEFAULT_REPORT_INTERVAL)
	, m_enabled(false)
{
	setDefaults();
}

void HubTermConfig::setDefaults() {
	m_centerUrl = QStringLiteral("ws://127.0.0.1:8097/api/ws/agent");
	m_nodeId = QString();
	m_nodeName = QSysInfo::machineHostName();
	m_token = QString();
	m_domain = QStringLiteral("default");
	m_reportInterval = DEFAULT_REPORT_INTERVAL;
	m_enabled = false;
}

bool HubTermConfig::load(const QString &filePath /*= QString()*/) {
	QString configPath = filePath;

	if (configPath.isEmpty()) {
		QString appDir = QCoreApplication::applicationDirPath();
		configPath = appDir + QDir::separator() + CONFIG_FILE_NAME;
	}

	QFile file(configPath);

	if (!file.exists()) {
		m_configFilePath = configPath;
		setDefaults();
		return false;
	}

	if (!file.open(QIODevice::ReadOnly)) {
		m_configFilePath = configPath;
		setDefaults();
		return false;
	}

	QByteArray data = file.readAll();
	file.close();

	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

	if (parseError.error != QJsonParseError::NoError) {
		m_configFilePath = configPath;
		setDefaults();
		return false;
	}

	m_configFilePath = configPath;
	fromJson(doc.object());
	return true;
}

bool HubTermConfig::save() {
	if (m_configFilePath.isEmpty()) {
		return false;
	}

	QJsonDocument doc(toJson());
	QFile file(m_configFilePath);

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return false;
	}

	file.write(doc.toJson(QJsonDocument::Indented));
	file.close();
	return true;
}

QJsonObject HubTermConfig::toJson() const {
	QJsonObject json;

	json[QStringLiteral("center_url")] = m_centerUrl;
	json[QStringLiteral("node_id")] = m_nodeId;
	json[QStringLiteral("node_name")] = m_nodeName;
	json[QStringLiteral("token")] = m_token;
	json[QStringLiteral("domain")] = m_domain;
	json[QStringLiteral("report_interval")] = m_reportInterval;
	json[QStringLiteral("enabled")] = m_enabled;
	return json;
}

void HubTermConfig::fromJson(const QJsonObject &json) {
	m_centerUrl = json.value(QStringLiteral("center_url")).toString(m_centerUrl);
	m_nodeId = json.value(QStringLiteral("node_id")).toString(m_nodeId);
	m_nodeName = json.value(QStringLiteral("node_name")).toString(m_nodeName);
	m_token = json.value(QStringLiteral("token")).toString(m_token);
	m_domain = json.value(QStringLiteral("domain")).toString(m_domain);
	m_reportInterval = json.value(QStringLiteral("report_interval")).toInt(DEFAULT_REPORT_INTERVAL);
	m_enabled = json.value(QStringLiteral("enabled")).toBool(false);
}
