#ifndef LOCKSETTINGSWINDOW_H
#define LOCKSETTINGSWINDOW_H

#include <QDialog>

namespace Ui {
class LockSettingsWindow;
}
class Locker;

class LockSettingsWindow : public QDialog
{
    Q_OBJECT
    
public:
    explicit LockSettingsWindow(Locker *locker, QWidget *parent = 0);
    ~LockSettingsWindow();
    
private:
    Ui::LockSettingsWindow *ui;
    Locker *_locker;
};

#endif // LOCKSETTINGSWINDOW_H
