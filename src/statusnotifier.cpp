#include "statusnotifier.h"

#include <QApplication>
#include <QWindow>
#include <QDebug>
#ifdef Q_OS_LINUX
#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusPendingCall>
#endif
#include <stdlib.h>

void onShow(GtkCheckMenuItem *menu, gpointer data)
{
    bool checked = gtk_check_menu_item_get_active(menu);
    QWindow *w = static_cast<QApplication *>(data)->topLevelWindows().at(0);
    if (checked) {
        w->show();
    } else {
        w->hide();
    }
}

void onQuit(GtkMenu *, gpointer data)
{
    static_cast<QApplication *>(data)->quit();
}

StatusNotifier::StatusNotifier(QObject *parent) :
    QObject(parent)
{
    QString de(getenv("XDG_CURRENT_DESKTOP"));
    useAppIndicator = appIndicatorDE.contains(de, Qt::CaseInsensitive);

    systray = new QSystemTrayIcon(this);
    if (isUsingAppIndicator()) {
        createAppIndicator();
    } else {
        createSystemTray();
    }
}

StatusNotifier::~StatusNotifier()
{
#ifdef Q_OS_WIN
    systray->hide();
#endif
}

const QStringList StatusNotifier::appIndicatorDE = QStringList() << "Unity" << "XFCE" << "Pantheon";

bool StatusNotifier::isUsingAppIndicator() const
{
    return useAppIndicator;
}

void StatusNotifier::createAppIndicator()
{
    AppIndicator *indicator = app_indicator_new("Shadowsocks-Qt5", "shadowsocks-qt5", APP_INDICATOR_CATEGORY_OTHER);
    GtkWidget *menu = gtk_menu_new();

    showItem = gtk_check_menu_item_new_with_label(tr("Show").toLocal8Bit().constData());
    gtk_check_menu_item_set_active((GtkCheckMenuItem*)showItem, true);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), showItem);
    g_signal_connect(showItem, "toggled", G_CALLBACK(onShow), qApp);
    gtk_widget_show(showItem);

    GtkWidget *exitItem = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), exitItem);
    g_signal_connect(exitItem, "activate", G_CALLBACK(onQuit), qApp);
    gtk_widget_show(exitItem);

    app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_menu(indicator, GTK_MENU(menu));
}

void StatusNotifier::createSystemTray()
{
    systrayMenu = new QMenu;
    minimiseRestoreAction = new QAction(tr("Minimise"), this);
    connect(minimiseRestoreAction, &QAction::triggered, this, &StatusNotifier::activated);
    systrayMenu->addAction(minimiseRestoreAction);
    systrayMenu->addAction(QIcon::fromTheme("application-exit", QIcon::fromTheme("exit")), tr("Quit"), qApp, SLOT(quit()));

    systray->setIcon(QIcon(":/icons/icons/shadowsocks-qt5.png"));
    systray->setToolTip(QString("Shadowsocks-Qt5"));
    systray->setContextMenu(systrayMenu);
    connect(systray, &QSystemTrayIcon::activated, [this] (QSystemTrayIcon::ActivationReason r) {
        if (r != QSystemTrayIcon::Context) {
            emit activated();
        }
    });
    systray->show();
}

void StatusNotifier::onMainWindowVisibilityChanged(const bool visible)
{
    if (isUsingAppIndicator()) {
        if (visible) {
            gtk_check_menu_item_set_active((GtkCheckMenuItem*)showItem, true);
        } else {
            gtk_check_menu_item_set_active((GtkCheckMenuItem*)showItem, false);
        }
    } else {
        minimiseRestoreAction->setText(visible ? tr("Minimise") : tr("Restore"));
    }
}

void StatusNotifier::showNotification(const QString &msg)
{
#ifdef Q_OS_LINUX
    //Using DBus to send message.
    QDBusMessage method = QDBusMessage::createMethodCall("org.freedesktop.Notifications","/org/freedesktop/Notifications", "org.freedesktop.Notifications", "Notify");
    QVariantList args;
    args << QCoreApplication::applicationName() << quint32(0) << "shadowsocks-qt5" << "Shadowsocks-Qt5" << msg << QStringList () << QVariantMap() << qint32(-1);
    method.setArguments(args);
    QDBusConnection::sessionBus().asyncCall(method);
#else
    systray->showMessage("Shadowsocks-Qt5", msg);
#endif
}