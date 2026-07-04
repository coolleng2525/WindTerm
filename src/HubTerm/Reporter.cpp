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
#include <QThread>

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

	info[QStringLiteral("cpu_count")] = QThread::idealThreadCount();

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

QJsonObject HubTermReporter::collectSystemMetrics() {
	QJsonObject metrics;
	quint64 memoryTotal = 0;
	quint64 memoryUsed = 0;

#ifdef Q_OS_LINUX
	QFile memInfo(QStringLiteral("/proc/meminfo"));
	if (memInfo.open(QIODevice::ReadOnly)) {
		QTextStream in(&memInfo);
		QString line;
		quint64 memoryAvailable = 0;

		while (in.readLineInto(&line)) {
			const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
			if (parts.size() < 2) {
				continue;
			}
			if (parts[0] == QLatin1String("MemTotal:")) {
				memoryTotal = parts[1].toULongLong() * 1024;
			} else if (parts[0] == QLatin1String("MemAvailable:")) {
				memoryAvailable = parts[1].toULongLong() * 1024;
			}
		}
		if (memoryTotal > memoryAvailable) {
			memoryUsed = memoryTotal - memoryAvailable;
		}
	}
#endif

	QStorageInfo root = QStorageInfo::root();
	const quint64 diskTotal = root.bytesTotal() > 0 ? static_cast<quint64>(root.bytesTotal()) : 0;
	const quint64 diskFree = root.bytesAvailable() > 0 ? static_cast<quint64>(root.bytesAvailable()) : 0;
	const quint64 diskUsed = diskTotal > diskFree ? diskTotal - diskFree : 0;

	metrics[QStringLiteral("cpu_percent")] = 0.0;
	metrics[QStringLiteral("memory_total")] = static_cast<double>(memoryTotal);
	metrics[QStringLiteral("memory_used")] = static_cast<double>(memoryUsed);
	metrics[QStringLiteral("memory_percent")] = memoryTotal > 0 ? static_cast<double>(memoryUsed) / static_cast<double>(memoryTotal) * 100.0 : 0.0;
	metrics[QStringLiteral("disk_total")] = static_cast<double>(diskTotal);
	metrics[QStringLiteral("disk_used")] = static_cast<double>(diskUsed);
	return metrics;
}

QJsonArray HubTermReporter::collectSerialPorts() {
	QJsonArray ports;
	QStringList portPaths = scanSerialPorts();

	for (const QString &path : portPaths) {
		QJsonObject port;
		port[QStringLiteral("port_name")] = path;
		port[QStringLiteral("description")] = QFileInfo(path).fileName();
		port[QStringLiteral("status")] = QStringLiteral("online");
		port[QStringLiteral("baud_rate")] = 0;
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
