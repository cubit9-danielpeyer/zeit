/* ========================================================================
*    Copyright (C) 2015-2016 Blaze <blaze@vivaldi.net>
*
*    This file is part of Zeit.
*
*    Zeit is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    Zeit is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with Zeit.  If not, see <http://www.gnu.org/licenses/>.
* ======================================================================== */

#include "config.h"

#include <unistd.h>
#include <QApplication>
#include <QTranslator>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    if(getuid() == 0)
        qFatal("User should not be root");
    QApplication a(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Zeit"));
    QApplication::setOrganizationName(QStringLiteral("zeit"));
    QApplication::setApplicationVersion(QStringLiteral(ZEIT_V));
    QTranslator translator;
    translator.load(QApplication::applicationDirPath() +
                    QLatin1String("/../share/zeit/translations/") +
                    QLocale::system().name() + QLatin1String(".qm"));
    a.installTranslator(&translator);
    MainWindow w;
    w.show();
    return a.exec();
}
