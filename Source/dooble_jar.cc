/*
** Copyright (c) 2008 - present, Alexis Megas.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from Dooble without specific prior written permission.
**
** DOOBLE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** DOOBLE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include "dooble_jar.h"
#include "dooble_web_engine_view.h"

dooble_jar::dooble_jar(QObject *parent):QWebEngineUrlSchemeHandler(parent)
{
}

void dooble_jar::requestStarted(QWebEngineUrlRequestJob *request)
{
  if(!request || m_request == request)
    return;

  m_request = request;

  auto jar_implementation = new dooble_jar_implementation
    (m_request->requestUrl(), m_web_engine_view, m_request);

  connect(jar_implementation,
	  SIGNAL(error(QWebEngineUrlRequestJob::Error)),
	  this,
	  SLOT(slot_error(QWebEngineUrlRequestJob::Error)));
  connect(jar_implementation,
	  SIGNAL(finished(const QByteArray &, const bool)),
	  this,
	  SLOT(slot_finished(const QByteArray &, const bool)));
}

void dooble_jar::set_web_engine_view(dooble_web_engine_view *web_engine_view)
{
  m_web_engine_view = web_engine_view;
}

void dooble_jar::slot_error(QWebEngineUrlRequestJob::Error error)
{
  if(m_request)
    m_request->fail(error);
}

void dooble_jar::slot_finished(const QByteArray &bytes, const bool file)
{
  if(m_request)
    {
      if(bytes.isEmpty())
	m_request->fail(QWebEngineUrlRequestJob::RequestFailed);
      else
	{
	  if(file)
	    {
	      QUrl url;

	      url.setPath
		(QStandardPaths::
		 standardLocations(QStandardPaths::DesktopLocation).value(0) +
		 "/Dooble-Jar/" +
		 bytes);
	      url.setScheme("file");
	      m_web_engine_view->setUrl(url);
	    }
	  else
	    {
	      auto buffer = new QBuffer(m_request);

	      buffer->setData(bytes);
	      m_request->reply("text/html", buffer);
	    }
	}
    }
}

dooble_jar_implementation::dooble_jar_implementation
(const QUrl &url,
 dooble_web_engine_view *web_engine_view,
 QObject *parent):QProcess(parent)
{
  QDir().mkdir
    (QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).
     value(0) +
     QDir::separator() +
     "Dooble-Jar");
  connect(this,
	  SIGNAL(finished(int, QProcess::ExitStatus)),
	  this,
	  SLOT(slot_finished(int, QProcess::ExitStatus)));
  connect(this,
	  SIGNAL(readyReadStandardOutput(void)),
	  this,
	  SLOT(slot_ready_read(void)));
  m_url = url;
  m_web_engine_view = web_engine_view;
  setWorkingDirectory
    (QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).
     value(0) +
     QDir::separator() +
     "Dooble-Jar");

  if(m_url.hasQuery())
    start
      ("jar", QStringList() << "-f" << m_url.path() << "-x" << m_url.query());
  else
    start("jar", QStringList() << "-f" << m_url.path() << "-t" << "-v");
}

dooble_jar_implementation::~dooble_jar_implementation()
{
  kill();
}

void dooble_jar_implementation::slot_finished
(int exit_code, QProcess::ExitStatus exit_status)
{
  Q_UNUSED(exit_code);
  Q_UNUSED(exit_status);

  if(m_url.hasQuery())
    {
      /*
      ** Redirect the request.
      */

      emit finished(m_url.query().toUtf8(), true);
      return;
    }

  m_html = "<html>\n";
  m_html += "<head>\n";
  m_html += "<style>\n";
  m_html += "td {padding-left: 0px; padding-right: 10px}\n";
  m_html += "</style>\n";
  m_html += "</head>\n";
  m_html += "<body bgcolor=\"white\" style=\"font-family: monospace\">\n";
  m_html += "<title>";
  m_html += m_url.path().toUtf8();
  m_html += "</title>\n";

  if(QFileInfo(m_url.path()).isReadable())
    {
      m_html += "<table>\n";
      m_html += "<tr><th>Size</th><th>Date</th><th>File</th></tr>\n";

      foreach(const auto &i, m_content.split('\n'))
	if(i.trimmed().size() > 0)
	  {
	    m_html += "<tr>\n";

	    QString file("");
	    auto list(QString(i).trimmed().split(' '));

	    file = QString("<a href=\"jar://%1?%2\">%2</a>").
	      arg(m_url.path()).arg(list.value(list.size() - 1));
	    m_html += "<td>" + list.value(0).toUtf8() + "</td>\n";
	    m_html += "<td>" + list.mid(1, 6).join(' ').toUtf8() + "</td>\n";
	    m_html += "<td>" + file.toUtf8() + "</td>\n";
	    m_html += "</tr>\n";
	  }

      m_html += "</table>\n";
    }
  else
    m_html += "The file " + m_url.path().toUtf8() + " is not readable.\n";

  m_html += "</body></html>";
  emit finished(m_html, false);
}

void dooble_jar_implementation::slot_ready_read(void)
{
  while(bytesAvailable() > 0)
    m_content.append(readAll());
}
