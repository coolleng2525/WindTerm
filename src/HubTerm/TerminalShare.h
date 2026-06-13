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

#ifndef HUBTERM_TERMINAL_SHARE_H
#define HUBTERM_TERMINAL_SHARE_H

#include <QObject>
#include <QWebSocket>
#include <QByteArray>

class Pty;

class TerminalShare
	: public QObject
{
	Q_OBJECT

public:
	explicit TerminalShare(Pty *pty, QWebSocket *ws, QObject *parent = nullptr);
	~TerminalShare();

	void setWritable(bool writable);
	bool isWritable() const;
	void setReadonly(bool readonly);
	bool isReadonly() const;

public slots:
	void onPtyReadyRead();
	void onRemoteWrite(const QByteArray &data);
	void onPtyDestroyed();

private:
	Pty *m_pty;
	QWebSocket *m_ws;
	bool m_writable;
	bool m_readonly;
};

#endif // HUBTERM_TERMINAL_SHARE_H
