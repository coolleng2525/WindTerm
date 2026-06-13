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

#ifndef HUBTERM_REPORTER_H
#define HUBTERM_REPORTER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>

class HubTermReporter
	: public QObject
{
	Q_OBJECT

public:
	explicit HubTermReporter(QObject *parent = nullptr);
	~HubTermReporter() = default;

	QJsonObject collectSystemInfo();
	QJsonArray collectSerialPorts();
	QJsonObject collectCapabilities();

signals:
	void reportReady(const QJsonObject &report);

private:
	QStringList scanSerialPorts() const;
	QString osName() const;
	QString cpuArchitecture() const;
};

#endif // HUBTERM_REPORTER_H
