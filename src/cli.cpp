// The command shape follows Hype's src/cli.cpp (MIT, David Heinemeier Hansson).
#include "cli.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QtConcurrentMap>
#include <cstdio>

#include "resizer.h"

namespace {
void print(FILE *stream, const QString &text) { fprintf(stream, "%s\n", qPrintable(text)); }
void print(const QJsonObject &object) { fprintf(stdout, "%s", QJsonDocument(object).toJson().constData()); }
int fail(const QString &message) {
    print(stderr, message);
    return 1;
}

// Expands folders into the images directly inside them and drops duplicates.
QStringList expand(const QStringList &arguments, QStringList *missing) {
    QStringList files;
    QSet<QString> seen;
    for (const auto &argument : arguments) {
        const QFileInfo info(argument);
        const QStringList found = info.isDir() ? Resizer::imagesIn(info.absoluteFilePath())
                                  : info.isFile() ? QStringList{info.absoluteFilePath()}
                                                  : QStringList{};
        if (found.isEmpty() && !info.isDir())
            missing->append(argument);
        for (const auto &file : found)
            if (!seen.contains(file)) {
                seen.insert(file);
                files << file;
            }
    }
    return files;
}

int info(const QStringList &arguments) {
    QCommandLineParser parser;
    parser.setApplicationDescription("Print each image's size.");
    parser.addHelpOption();
    parser.addOption({"json", "Print JSON."});
    parser.addPositionalArgument("images", "Image files or folders.", "<image|folder>...");
    parser.process(arguments);

    QStringList missing;
    const QStringList files = expand(parser.positionalArguments().mid(1), &missing);
    if (files.isEmpty() && missing.isEmpty())
        return fail("Give at least one image or folder.");

    QJsonArray images;
    bool ok = missing.isEmpty();
    for (const auto &path : missing)
        print(stderr, "Not found: " + path);
    for (const auto &file : files) {
        const auto probe = Resizer::probe(file);
        QJsonObject image{{"path", file}};
        if (probe.error.isEmpty()) {
            image["width"] = probe.size.width();
            image["height"] = probe.size.height();
            if (!parser.isSet("json"))
                print(stdout, QStringLiteral("%1\t%2x%3").arg(file).arg(probe.size.width()).arg(probe.size.height()));
        } else {
            ok = false;
            image["error"] = probe.error;
            print(stderr, file + ": " + probe.error);
        }
        images.append(image);
    }
    if (parser.isSet("json"))
        print(QJsonObject{{"images", images}});
    return ok ? 0 : 1;
}

int resize(const QStringList &arguments) {
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Write resized copies of images into a folder. The height follows the width, keeping each "
        "image's shape. Originals are never changed, and existing files are never replaced: a taken "
        "name gets the width added (photo-1200w.jpg).");
    parser.addHelpOption();
    parser.addOption({{"w", "width"}, "The new width in pixels.", "pixels"});
    parser.addOption({{"o", "out"}, "The folder to write into (created if missing).", "folder"});
    parser.addOption({"json", "Print JSON."});
    parser.addPositionalArgument("images", "Image files or folders.", "<image|folder>...");
    parser.process(arguments);

    bool numeric = false;
    const int width = parser.value("width").toInt(&numeric);
    if (!numeric || width <= 0)
        return fail("Give the new width in pixels with --width.");
    if (parser.value("out").isEmpty())
        return fail("Give the folder to write into with --out.");
    const QString folder = QFileInfo(parser.value("out")).absoluteFilePath();
    if (!QDir().mkpath(folder) || !QFileInfo(folder).isWritable())
        return fail("Can't write into " + folder);

    QStringList missing;
    const QStringList files = expand(parser.positionalArguments().mid(1), &missing);
    for (const auto &path : missing)
        print(stderr, "Not found: " + path);
    if (files.isEmpty())
        return fail("No images to resize.");

    QSet<QString> taken;
    QList<Resizer::Job> jobs;
    for (const auto &file : files)
        jobs.append({file, Resizer::destinationFor(file, folder, width, taken), width});
    const auto results = QtConcurrent::blockingMapped(jobs, &Resizer::resize);

    QJsonArray saved, failed;
    for (const auto &result : results) {
        if (result.error.isEmpty()) {
            saved.append(QJsonObject{{"source", result.source}, {"destination", result.destination},
                                     {"width", result.size.width()}, {"height", result.size.height()}});
            if (!parser.isSet("json"))
                print(stdout, QStringLiteral("%1 -> %2 (%3x%4)").arg(result.source, result.destination)
                                  .arg(result.size.width()).arg(result.size.height()));
        } else {
            failed.append(QJsonObject{{"source", result.source}, {"error", result.error}});
            print(stderr, result.error);
        }
    }
    if (parser.isSet("json"))
        print(QJsonObject{{"saved", saved}, {"failed", failed}});
    return failed.isEmpty() && missing.isEmpty() ? 0 : 1;
}

int skill(const QStringList &arguments) {
    QCommandLineParser parser;
    parser.setApplicationDescription("Print the agent skill, or install it for coding agents with: skill install.");
    parser.addHelpOption();
    parser.addPositionalArgument("install", "Copy to ~/.agents/skills/sizer and link into ~/.claude/skills", "[install]");
    parser.process(arguments);

    QFile bundled(":/skill.md");
    if (!bundled.open(QIODevice::ReadOnly))
        return fail("The bundled skill is missing.");
    const QByteArray text = bundled.readAll();
    const QString action = parser.positionalArguments().value(1);
    if (action.isEmpty()) {
        fputs(text.constData(), stdout);
        return 0;
    }
    if (action != "install")
        return fail("Use sizer skill to print the skill, or sizer skill install.");

    const QDir home = QDir::home();
    const QString directory = home.filePath(".agents/skills/sizer");
    QSaveFile file(directory + "/SKILL.md");
    if (!QDir().mkpath(directory) || !file.open(QIODevice::WriteOnly) || file.write(text) < 0 || !file.commit())
        return fail("Could not write " + file.fileName());
    print(stdout, "Installed " + file.fileName());
    if (!home.exists(".claude"))
        return 0;
    const QString link = home.filePath(".claude/skills/sizer");
    const QFileInfo existing(link);
    if (existing.isSymLink())
        QFile::remove(link);
    else if (existing.exists())
        return fail(link + " exists and is not a link; leaving it alone.");
    if (!home.mkpath(".claude/skills") || !QFile::link("../../.agents/skills/sizer", link))
        return fail("Could not link " + link);
    print(stdout, "Linked " + link);
    return 0;
}

int help() {
    print(stdout, "Usage: sizer <command> [options]\n\n"
                  "  open [image|folder]...                  Open the window, optionally with images\n"
                  "  resize --width N --out DIR <image>...   Write resized copies into DIR\n"
                  "  info <image|folder>...                  Print each image's size\n"
                  "  skill [install]                         Print the skill for coding agents, or install it\n\n"
                  "resize and info take --json. Run a command with --help for its options.");
    return 0;
}
} // namespace

bool isCliCommand(const QString &word) {
    return QStringList{"resize", "info", "skill", "help", "--help", "-h"}.contains(word);
}

int runCli(const QStringList &arguments) {
    const QString command = arguments.value(1);
    return command == "resize" ? resize(arguments)
         : command == "info"   ? info(arguments)
         : command == "skill"  ? skill(arguments)
                               : help();
}
