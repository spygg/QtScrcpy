// Headless encoder test: connects to a device using a chosen video encoder and
// reports whether the scrcpy-server stream stays up for a few seconds.
// Usage: encoder_test <encoderName|empty> [serial]
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <QObject>

#include "QtScrcpyCore.h"
#include "QtScrcpyCoreDef.h"

using namespace qsc;

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    const QStringList args = a.arguments();
    if (args.size() < 2) {
        qDebug() << "usage: encoder_test <encoderName|empty> [serial]";
        return 2;
    }
    const QString codec = args.at(1);
    const QString serial = (args.size() > 2) ? args.at(2) : QStringLiteral("YY001094");

    DeviceParams params;
    params.serial = serial;
    params.serverLocalPath = QStringLiteral("G:/QtScrcpy/QtScrcpy/QtScrcpyCore/src/third_party/scrcpy-server");
    params.serverRemotePath = QStringLiteral("/data/local/tmp/scrcpy-server.jar");
    params.maxSize = 800;
    params.bitRate = 2000000;
    params.videoSource = VIDEO_SOURCE_DISPLAY;
    params.display = true;
    params.codecName = codec;
    params.scid = 0x3039;
    params.pushFilePath = QStringLiteral("/sdcard/");

    // Outcomes. The deviceConnected(success=true) signal fires as soon as the
    // server starts, BEFORE the encoder is actually exercised, so it cannot be
    // used as a "stable" verdict. Instead: any deviceDisconnected during the
    // observation window means the stream died (encoder crash); if we survive
    // the full window without a disconnect, the encoder is stable.
    QString result;
    bool verdict = false;

    QObject::connect(&IDeviceManage::getInstance(), &IDeviceManage::deviceConnected,
                     [&](bool success, const QString &s, const QString &name, const QSize &size) {
        qDebug() << "[connect] success=" << success << "serial=" << s
                 << "name=" << name << "size=" << size;
        if (!success) {
            result = QStringLiteral("CONNECT_FAILED codec=[%1]").arg(codec);
            verdict = false;
            a.quit();
        }
    });

    QObject::connect(&IDeviceManage::getInstance(), &IDeviceManage::deviceDisconnected,
                     [&](QString s) {
        qDebug() << "[disconnect] serial=" << s;
        // Any drop during the test window means the encoder died.
        result = QStringLiteral("CRASHED codec=[%1]").arg(codec);
        verdict = false;
        a.quit();
    });

    // Max observation window: if we stay connected this long, declare stable.
    QTimer::singleShot(15000, &a, [&]() {
        result = QStringLiteral("STABLE codec=[%1] (survived 15s)").arg(codec);
        verdict = true;
        a.quit();
    });

    qDebug() << "=== TEST codec=[" << codec << "] serial=" << serial << "===";
    if (!IDeviceManage::getInstance().connectDevice(params)) {
        qDebug() << "[connect] connectDevice returned false immediately";
    }

    a.exec();
    qDebug() << "RESULT:" << result;
    // Best-effort cleanup so the device isn't left streaming.
    IDeviceManage::getInstance().disconnectDevice(serial);
    return verdict ? 0 : 1;
}
