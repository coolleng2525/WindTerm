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

#ifndef HUBTERM_CONFIG_H
#define HUBTERM_CONFIG_H

#include <QObject>
#include <QString>
#include <QJsonObject>

class HubTermConfig
	: public QObject
{
	Q_OBJECT

public:
	explicit HubTermConfig(QObject *parent = nullptr);
	~HubTermConfig() = default;

	static HubTermConfig *instance();

	bool load(const QString &filePath = QString());
	bool save();

	QString centerUrl() const { return m_centerUrl; }
	void setCenterUrl(const QString &url) { m_centerUrl = url; }

	QString nodeId() const { return m_nodeId; }
	void setNodeId(const QString &id) { m_nodeId = id; }

	QString nodeName() const { return m_nodeName; }
	void setNodeName(const QString &name) { m_nodeName = name; }

	QString token() const { return m_token; }
	void setToken(const QString &token) { m_token = token; }

	int reportInterval() const { return m_reportInterval; }
	void setReportInterval(int ms) { m_reportInterval = ms; }

	QString domain() const { return m_domain; }
	void setDomain(const QString &domain) { m_domain = domain; }

	bool enabled() const { return m_enabled; }
	void setEnabled(bool enabled) { m_enabled = enabled; }

	QString configFilePath() const { return m_configFilePath; }

private:
	void setDefaults();
	QJsonObject toJson() const;
	void fromJson(const QJsonObject &json);

	QString m_centerUrl;
	QString m_nodeId;
	QString m_nodeName;
	QString m_token;
	QString m_domain;
	QString m_configFilePath;
	int m_reportInterval;
	bool m_enabled;
};

#endif // HUBTERM_CONFIG_H
