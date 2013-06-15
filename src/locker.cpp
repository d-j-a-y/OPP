#include "locker.h"
#include "passformdialog.h"

#include <QWidget>
#include <QDebug>

Locker::Locker(QList<QWidget*> widgets, QObject *parent) :
    QObject(parent),
    _widgets(widgets)
{
    _passDialog = new passformdialog(this);
    this->setPasswordEnable(false);
}

bool Locker::getAutoLock(){
    return _autoLock;
}

void Locker::setAutoLock(bool lock){
    _autoLock = lock;
    if (lock == false){
        _timer.stop();
    }
}

bool Locker::passwordEnable(){
    return _password;
}

void Locker::setPasswordEnable(bool pass){
    _password = pass;
}

QString Locker::getPassword(){
    return _thePass;
}

void Locker::setPassword(QString newPass){
    if (passwordEnable() == true) {
        _thePass = newPass;
    }
}

void Locker::setAutoLockDelay(int time){
    if (getAutoLock() == true) {
        _timer.start(time);
    }
}

void Locker::lock()
{
    foreach(QWidget *widget, _widgets) {
        widget->setEnabled(false);
    }
}

void Locker::unlock()
{
    if (this->passwordEnable()) {
        _passDialog->show();
        _passDialog->raise();
        _passDialog->activateWindow();
    } else {
        foreach(QWidget *widget, _widgets) {
            widget->setEnabled(true);
        }
    }
}

void Locker::toggle(bool checked)
{
    checked ? lock() : unlock();

    emit toggled(checked);
}

QList<QWidget*> Locker::getWidgets()
{
    return _widgets;
}
