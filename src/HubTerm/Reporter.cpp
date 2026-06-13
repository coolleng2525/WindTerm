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

#include "Reporter.h"

#include <QDir>
#include <QFileInfo>
#include <QSysInfo>
#include <QStorageInfo>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QRegularExpression>

#ifdef Q_OS_UNIX
#include <unistd.h>
#include <sys/utsname.h>
#endif

HubTermReporter::HubTermReporter(QObject *parent /*= nullptr*/)
	: QObject(parent)
{}

QString HubTermReporter::osName() const {
#ifdef Q_OS_WIN
	return QStringLiteral("Windows");
#elif defined(Q_OS_MACOS)
	return QStringLiteral("macOS");
#elif defined(Q_OS_LINUX)
	return QStringLiteral("Linux");
#elif defined(Q_OS_FREEBSD)
	return QStringLiteral("FreeBSD");
#else
	return QStringLiteral("Unknown");
#endif
}

QString HubTermReporter::cpuArchitecture() const {
	return QSysInfo::currentCpuArchitecture();
}

QStringList HubTermReporter::scanSerialPorts() const {
	QStringList ports;

#ifdef Q_OS_LINUX
	QDir ttyDir(QStringLiteral("/dev"));

	QStringList filters;
	filters << QStringLiteral("ttyUSB*") << QStringLiteral("ttyACM*") << QStringLiteral("ttyS*");
	QFileInfoList entries = ttyDir.entryInfoList(filters, QDir::System | QDir::Files);

	for (const QFileInfo &info : entries) {
		ports.append(info.absoluteFilePath());
	}
#elif defined(Q_OS_MACOS)
	QDir devDir(QStringLiteral("/dev"));
	QStringList filters;
	filters << QStringLiteral("tty.*") << QStringLiteral("cu.*");
	QFileInfoList entries = devDir.entryInfoList(filters, QDir::System | QDir::Files);

	for (const QFileInfo &info : entries) {
		ports.append(info.absoluteFilePath());
	}
#elif defined(Q_OS_WIN)
	// On Windows, serial ports are enumerated via registry or CreateFile
	// For now, return a placeholder; the closed-source GUI handles this
	Q_UNUSED(ports)
#endif

	return ports;
}

QJsonObject HubTermReporter::collectSystemInfo() {
	QJsonObject info;

	info[QStringLiteral("hostname")] = QSysInfo::machineHostName();
	info[QStringLiteral("os")] = osName();
	info[QStringLiteral("os_version")] = QSysInfo::productVersion();
	info[QStringLiteral("kernel")] = QSysInfo::kernelType() + QStringLiteral(" ") + QSysInfo::kernelVersion();
	info[QStringLiteral("architecture")] = cpuArchitecture();
	info[QStringLiteral("pretty_product")] = QSysInfo::prettyProductName();

#ifdef Q_OS_UNIX
	struct utsname uts;
	if (uname(&uts) == 0) {
		info[QStringLiteral("kernel_release")] = QString::fromLatin1(uts.release);
		info[QStringLiteral("machine")] = QString::fromLatin1(uts.machine);
	}
#endif

	// CPU count
	info[QStringLiteral("cpu_count")] = static_cast<int>(QSysInfo::numberOfCpus().value_or(0));

	// Memory info (approximate)
#ifdef Q_OS_LINUX
	QFile memInfo(QStringLiteral("/proc/meminfo"));
	if (memInfo.open(QIODevice::ReadOnly)) {
		QTextStream in(&memInfo);
		QString line;

		while (in.readLineInto(&line)) {
			if (line.startsWith(QStringLiteral("MemTotal:"))) {
				QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")));
				if (parts.size() >= 2) {
					info[QStringLiteral("memory_kb")] = parts[1].toLongLong();
				}
				break;
			}
		}
		memInfo.close();
	}
#endif

	return info;
}

QJsonArray HubTermReporter::collectSerialPorts() {
	QJsonArray ports;
	QStringList portPaths = scanSerialPorts();

	for (const QString &path : portPaths) {
		QJsonObject port;
		port[QStringLiteral("path")] = path;
		port[QStringLiteral("name")] = QFileInfo(path).fileName();
		port[QStringLiteral("available")] = true;
		ports.append(port);
	}

	return ports;
}

QJsonObject HubTermReporter::collectCapabilities() {
	QJsonObject caps;

	caps[QStringLiteral("serial")] = true;
	caps[QStringLiteral("ssh")] = true;
	caps[QStringLiteral("telnet")] = true;
	caps[QStringLiteral("terminal_sharing")] = true;
	caps[QStringLiteral("script_execution")] = true;

	QJsonObject versions;
	versions[QStringLiteral("hubterm_agent")] = QStringLiteral("1.0.0");
	caps[QStringLiteral("versions")] = versions;

	return caps;
}
