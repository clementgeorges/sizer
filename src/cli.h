#pragma once

#include <QStringList>

// Commands that need no display, for scripts and coding agents:
// sizer resize, sizer info, sizer skill. The window stays behind sizer open.
bool isCliCommand(const QString &word);
int runCli(const QStringList &arguments);
