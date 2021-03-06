/* ========================================================================
*    Copyright (C) 2015-2020 Blaze <blaze@vivaldi.net>
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

#include <QMessageBox>
#include <QKeyEvent>
#include <QProcess>

#ifdef BUILD_HELPER
  #define ROOT_ACTIONS cron->isSystemCron()
#else
  #define ROOT_ACTIONS false
#endif // BUILD_HELPER

#include "cthost.h"
#include "ctcron.h"
#include "cttask.h"
#include "ctvariable.h"
#include "ctInitializationError.h"

#include "aboutdialog.h"
#include "alarmdialog.h"
#include "taskdialog.h"
#include "commanddialog.h"
#include "timerdialog.h"
#include "variabledialog.h"
#include "commands.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

#define QSL QStringLiteral


MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent),
    ui(new Ui::MainWindow),
    commands(new Commands())
{
    ui->setupUi(this);
    ui->mainToolBar->setMovable(false);
    ui->mainToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ui->filterEdit->hide();
    ui->hideFilterButton->hide();
    ui->labelWarning->hide();
    ui->actionAddEntry->setIcon(QIcon::fromTheme(
                                    QStringLiteral("document-new"),
                                    QIcon(QSL(":/icons/document-new"))));
    ui->actionModifyEntry->setIcon(QIcon::fromTheme(
                                       QStringLiteral("document-edit"),
                                       QIcon(QSL(":/icons/document-edit"))));
    ui->actionDeleteEntry->setIcon(QIcon::fromTheme(
                                       QStringLiteral("document-close"),
                                       QIcon(QSL(":/icons/document-close"))));
    ui->actionQuit->setIcon(QIcon::fromTheme(
                                QStringLiteral("application-exit"),
                                QIcon(QSL(":/icons/application-exit"))));
    ui->actionAlarm->setIcon(QIcon::fromTheme(
                                 QStringLiteral("chronometer"),
                                 QIcon(QSL(":/icons/chronometer"))));
    ui->actionTimer->setIcon(QIcon::fromTheme(
                                 QStringLiteral("player-time"),
                                 QIcon(QSL(":/icons/player-time"))));
    /* init crontab host */
    CTInitializationError error;
    ctHost = new CTHost(QStringLiteral("crontab"), error);
    selectUser(false);
    /* check if `at` binary is available */
    QProcess proc;
    proc.start(QStringLiteral("which"), QStringList{QStringLiteral("at")});
    proc.waitForFinished(-1);
    ui->actionCommands->setDisabled(proc.readAllStandardOutput().isEmpty());
    /* window actions */
    ui->mainToolBar->addAction(ui->actionAddEntry);
    ui->mainToolBar->addAction(ui->actionModifyEntry);
    ui->mainToolBar->addAction(ui->actionDeleteEntry);
    ui->mainToolBar->addAction(ui->actionAlarm);
    ui->mainToolBar->addAction(ui->actionTimer);
    auto group = new QActionGroup(this);
    ui->actionTasks->setActionGroup(group);
    ui->actionVariables->setActionGroup(group);
    ui->actionCommands->setActionGroup(group);
    toggleItemAction = new QAction(this);
    toggleItemAction->setText(tr("To&ggle"));
    toggleItemAction->setShortcut(QStringLiteral("Ctrl+G"));
    connect(toggleItemAction, &QAction::triggered, this, [this] {
        int index = ui->listWidget->currentRow();
        if(ui->actionTasks->isChecked()) {
            CTTask* task = cron->tasks().at(index);
            task->enabled = !task->enabled;
            cron->save();
            setIcon(ui->listWidget->item(index), task->enabled);
        }
        if(ui->actionVariables->isChecked()) {
            CTVariable* var = cron->variables().at(index);
            var->enabled = !var->enabled;
            cron->save();
            setIcon(ui->listWidget->item(index), var->enabled);
        }
    });
    ui->listWidget->addAction(toggleItemAction);
    ui->listWidget->addAction(ui->actionModifyEntry);
    ui->listWidget->addAction(ui->actionDeleteEntry);
    ui->listWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
    ui->listWidget->setWordWrap(ui->actionWrapText->isChecked());
    connect(ui->listWidget, &QListWidget::itemSelectionChanged, this, [this] {
        refreshActions(ui->listWidget->currentRow() > -1
                       && ui->listWidget->currentItem()->isSelected());
    });
    connect(ui->listWidget, &QListWidget::itemDoubleClicked,
            toggleItemAction, &QAction::trigger);
    connect(ui->hideFilterButton, &QPushButton::released,
            ui->actionShowFilter, &QAction::toggle);
    connect(ui->filterEdit, &QLineEdit::textEdited,
            this, [this] (const QString& text) {
        for(int i = 0; i < ui->listWidget->count(); i++) {
            QListWidgetItem* item = ui->listWidget->item(i);
            item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
        }
    });
    // Main menu
    connect(ui->actionAddEntry, &QAction::triggered,
            this, &MainWindow::addEntry);
    connect(ui->actionModifyEntry, &QAction::triggered,
            this, &MainWindow::modifyEntry);
    connect(ui->actionDeleteEntry, &QAction::triggered,
            this, &MainWindow::deleteEntry);
    connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::close);
    // View menu
    connect(ui->actionRefresh, &QAction::triggered, this, [this] {
        if(ui->actionTasks->isChecked())
            showTasks();
        if(ui->actionVariables->isChecked())
            showVariables();
        if(ui->actionCommands->isChecked())
            showCommands();
    });
    connect(ui->actionSystem, &QAction::triggered, this, [this] (bool check) {
        selectUser(check);
        ui->actionRefresh->trigger();
    });
    connect(ui->actionTasks, &QAction::triggered, this, &MainWindow::viewTasks);
    connect(ui->actionVariables, &QAction::triggered,
            this, &MainWindow::viewVariables);
    connect(ui->actionCommands, &QAction::triggered,
            this, &MainWindow::viewCommands);
    connect(ui->actionShowFilter, &QAction::toggled, this, [this] (bool check) {
       ui->filterEdit->setVisible(check);
       ui->hideFilterButton->setVisible(check);
       if(check) {
           ui->filterEdit->setFocus();
       } else {
           ui->filterEdit->clear();
           emit ui->filterEdit->textEdited(QString());
       }
    });
    connect(ui->actionWrapText, &QAction::toggled,
            ui->listWidget, &QListWidget::setWordWrap);
    // Tools menu
    connect(ui->actionAlarm, &QAction::triggered,
            this, &MainWindow::showAlarmDialog);
    connect(ui->actionTimer, &QAction::triggered,
            this, &MainWindow::showTimerDialog);
    // Help menu
    connect(ui->actionAbout, &QAction::triggered,
            this, &MainWindow::showAboutDialog);
    /* refresh state */
    viewTasks();
    refreshActions(false);
}

MainWindow::~MainWindow() {
    delete commands;
    delete ctHost;
    delete ui;
}

void MainWindow::keyPressEvent(QKeyEvent* e) {
    if(e->key() == Qt::Key_Escape && ui->filterEdit->isVisible())
        ui->actionShowFilter->trigger();
}

void MainWindow::refreshActions(bool enabled) {
    toggleItemAction->setDisabled(ui->actionCommands->isChecked() || !enabled);
    bool currentUser = cron->isCurrentUserCron() || cron->isSystemCron();
    ui->actionAddEntry->setEnabled(currentUser);
    ui->actionModifyEntry->setEnabled((currentUser && enabled)
                                      && !ui->actionCommands->isChecked());
    ui->actionDeleteEntry->setEnabled(currentUser && enabled);
    ui->actionAlarm->setEnabled(currentUser);
    ui->actionTimer->setEnabled(currentUser);
}

void MainWindow::setIcon(QListWidgetItem* item, bool enabled) {
    QString icon = enabled ? QSL("dialog-ok-apply") : QSL("edit-delete");
    item->setIcon(QIcon::fromTheme(icon, QIcon(QSL(":/icons/") + icon)));
}

void MainWindow::showTasks() {
    ui->listWidget->setEnabled(cron->isCurrentUserCron() || ROOT_ACTIONS);
    ui->labelWarning->setVisible(ROOT_ACTIONS);
    ui->listWidget->clear();
    for(CTTask* task: cron->tasks()) {
        QListWidgetItem* item = new QListWidgetItem();
        QString text;
        text.append(tr("Command: %1\n"
                       "Description: %2\n").arg(task->command, task->comment));
        text.append(tr("Runs %1",
                       "Runs at 'period described'").arg(task->describe()));
        item->setText(text);
        setIcon(item, task->enabled);
        ui->listWidget->addItem(item);
    }
    refreshActions(false);
}

void MainWindow::showVariables() {
    ui->listWidget->setEnabled(cron->isCurrentUserCron() || ROOT_ACTIONS);
    ui->labelWarning->setVisible(ROOT_ACTIONS);
    ui->listWidget->clear();
    for(CTVariable* var: cron->variables()) {
        QListWidgetItem* item = new QListWidgetItem();
        item->setText(QString(QStringLiteral("%1%2=%3"))
                      .arg(var->comment.isEmpty()
                           ? QString()
                           : QString(QSL("## %1\n")).arg(var->comment),
                           var->variable, var->value));
        setIcon(item, var->enabled);
        ui->listWidget->addItem(item);
    }
    refreshActions(false);
}

void MainWindow::showCommands() {
    ui->labelWarning->hide();
    ui->listWidget->setEnabled(true);
    ui->listWidget->clear();
    for(const Command& c: commands->getCommands()) {
        QListWidgetItem* item = new QListWidgetItem(
                    c.description + tr("\nCommand: ") + c.command);
        ui->listWidget->addItem(item);
    }
    refreshActions(false);
}

void MainWindow::selectUser(bool system) {
    cron = system ? ctHost->findSystemCron() : ctHost->findCurrentUserCron();
}

void MainWindow::addEntry() {
    if(ui->actionTasks->isChecked()) {
        CTTask* task = new CTTask(QString(), QString(),
                                  cron->userLogin(), false);
        TaskDialog *td = new TaskDialog(task, tr("New Task"), this);
        td->show();
        connect(td, &TaskDialog::accepted, this, [this, task] {
            cron->addTask(task);
            cron->save();
            if(ui->actionTasks->isChecked())
                showTasks();
        });
    }
    if(ui->actionVariables->isChecked()) {
        CTVariable* var = new CTVariable(QString(),
                                         QString(), cron->userLogin());
        VariableDialog* vd = new VariableDialog(var, tr("New Variable"), this);
        vd->show();
        connect(vd, &VariableDialog::accepted, this, [this, var] {
            cron->addVariable(var);
            cron->save();
            if(ui->actionVariables->isChecked())
                showVariables();
        });
    }
    if(ui->actionCommands->isChecked()) {
        CommandDialog* cd = new CommandDialog(commands, this);
        cd->show();
        connect(cd, &CommandDialog::accepted, this, &MainWindow::showCommands);
    }
}

void MainWindow::modifyEntry() {
    int index = ui->listWidget->currentRow();
    if(ui->actionTasks->isChecked()) {
        CTTask* task = cron->tasks().at(index);
        TaskDialog* td = new TaskDialog(task, tr("Edit Task"), this);
        td->show();
        connect(td, &TaskDialog::accepted, this, [this, task] {
            cron->modifyTask(task);
            cron->save();
            if(ui->actionTasks->isChecked())
                showTasks();
        });
    }
    if(ui->actionVariables->isChecked()) {
        CTVariable* var = cron->variables().at(index);
        VariableDialog* vd = new VariableDialog(var, tr("Edit Variable"), this);
        vd->show();
        connect(vd, &VariableDialog::accepted, this, [this, var] {
            cron->modifyVariable(var);
            cron->save();
            if(ui->actionVariables->isChecked())
                showVariables();
        });
    }
}

void MainWindow::deleteEntry() {
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this,
                                  tr("Deleting Entry"), tr("Delete entry?"),
                                  QMessageBox::Yes|QMessageBox::No);
    if(reply == QMessageBox::No)
        return;
    int index = ui->listWidget->currentRow();
    if(ui->actionTasks->isChecked()) {
        CTTask* task = cron->tasks().at(index);
        cron->removeTask(task);
        cron->save();
    }
    if(ui->actionVariables->isChecked()) {
        CTVariable* var = cron->variables().at(index);
        cron->removeVariable(var);
        cron->save();
    }
    if(ui->actionCommands->isChecked())
        commands->deleteCommand(index);
    ui->actionRefresh->trigger();
}

void MainWindow::viewTasks() {
    ui->actionAddEntry->setText(tr("Add Task"));
    ui->actionModifyEntry->setText(tr("Modify Task"));
    ui->actionDeleteEntry->setText(tr("Delete Task"));
    ui->actionSystem->setEnabled(true);
    showTasks();
    ui->listWidget->setToolTip(tr("crontab tasks, running periodically"));
}

void MainWindow::viewVariables() {
    ui->actionAddEntry->setText(tr("Add Variable"));
    ui->actionModifyEntry->setText(tr("Modify Variable"));
    ui->actionDeleteEntry->setText(tr("Delete Variable"));
    ui->actionSystem->setEnabled(true);
    showVariables();
    ui->listWidget->setToolTip(tr("environment variables for crontab"));
}

void MainWindow::viewCommands() {
    ui->actionAddEntry->setText(tr("Add Command"));
    ui->actionModifyEntry->setText(tr("Modify Command"));
    ui->actionDeleteEntry->setText(tr("Delete Command"));
    selectUser(false);
    ui->actionSystem->setChecked(false);
    ui->actionSystem->setEnabled(false);
    showCommands();
    ui->listWidget->setToolTip(tr("commands, scheduled to be executed once"));
}

void MainWindow::showAlarmDialog() {
    CTTask* task = new CTTask(QString(), QString(), cron->userLogin(), false);
    AlarmDialog* ad = new AlarmDialog(task, this);
    ad->show();
    connect(ad, &AlarmDialog::accepted, this, [this, task] {
        cron->addTask(task);
        cron->save();
        if(ui->actionTasks->isChecked())
            showTasks();
    });
}

void MainWindow::showTimerDialog() {
    TimerDialog* td = new TimerDialog(commands, this);
    td->show();
    connect(td, &TimerDialog::accepted, this, [this] {
        if(ui->actionCommands->isChecked())
            showCommands();
    });
}

void MainWindow::showAboutDialog() {
    ui->actionAbout->setDisabled(true);
    AboutDialog* about = new AboutDialog(this);
    about->show();
    connect(about, &AboutDialog::destroyed,
            this, [this] { ui->actionAbout->setEnabled(true); });
}
