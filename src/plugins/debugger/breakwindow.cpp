/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#include "breakwindow.h"

#include "debuggeractions.h"
#include "debuggerconstants.h"
#include "ui_breakcondition.h"
#include "ui_breakbyfunction.h"

#include <utils/qtcassert.h>
#include <utils/savedaction.h>

#include <QtCore/QDebug>

#include <QtGui/QAction>
#include <QtGui/QHeaderView>
#include <QtGui/QKeyEvent>
#include <QtGui/QMenu>
#include <QtGui/QResizeEvent>
#include <QtGui/QItemSelectionModel>
#include <QtGui/QToolButton>
#include <QtGui/QTreeView>


namespace Debugger {
namespace Internal {

///////////////////////////////////////////////////////////////////////
//
// BreakByFunctionDialog
//
///////////////////////////////////////////////////////////////////////

class BreakByFunctionDialog : public QDialog, Ui::BreakByFunctionDialog
{
public:
    explicit BreakByFunctionDialog(QWidget *parent)
      : QDialog(parent)
    {
        setupUi(this);
        connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
        connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    }
    QString functionName() const { return functionLineEdit->text(); }
};


///////////////////////////////////////////////////////////////////////
//
// BreakWindow
//
///////////////////////////////////////////////////////////////////////

BreakWindow::BreakWindow(QWidget *parent)
  : QTreeView(parent)
{
    m_alwaysResizeColumnsToContents = false;

    QAction *act = theDebuggerAction(UseAlternatingRowColors);
    setFrameStyle(QFrame::NoFrame);
    setAttribute(Qt::WA_MacShowFocusRect, false);
    setWindowTitle(tr("Breakpoints"));
    setWindowIcon(QIcon(QLatin1String(":/debugger/images/debugger_breakpoints.png")));
    setAlternatingRowColors(act->isChecked());
    setRootIsDecorated(false);
    setIconSize(QSize(10, 10));
    setSelectionMode(QAbstractItemView::ExtendedSelection);

    connect(this, SIGNAL(activated(QModelIndex)),
        this, SLOT(rowActivated(QModelIndex)));
    connect(act, SIGNAL(toggled(bool)),
        this, SLOT(setAlternatingRowColorsHelper(bool)));
    connect(theDebuggerAction(UseAddressInBreakpointsView), SIGNAL(toggled(bool)),
        this, SLOT(showAddressColumn(bool)));
}

BreakWindow::~BreakWindow()
{
}

void BreakWindow::showAddressColumn(bool on)
{
    setColumnHidden(7, !on);
}

static QModelIndexList normalizeIndexes(const QModelIndexList &list)
{
    QModelIndexList res;
    foreach (const QModelIndex &index, list)
        if (index.column() == 0)
            res.append(index);
    return res;
}

void BreakWindow::keyPressEvent(QKeyEvent *ev)
{
    if (ev->key() == Qt::Key_Delete) {
        QItemSelectionModel *sm = selectionModel();
        QTC_ASSERT(sm, return);
        QModelIndexList si = sm->selectedIndexes();
        if (si.isEmpty())
            si.append(currentIndex().sibling(currentIndex().row(), 0));
        deleteBreakpoints(normalizeIndexes(si));
    }
    QTreeView::keyPressEvent(ev);
}

void BreakWindow::resizeEvent(QResizeEvent *ev)
{
    QTreeView::resizeEvent(ev);
}

void BreakWindow::mouseDoubleClickEvent(QMouseEvent *ev)
{
    QModelIndex indexUnderMouse = indexAt(ev->pos());
    if (indexUnderMouse.isValid() && indexUnderMouse.column() >= 4)
        editBreakpoint(QModelIndexList() << indexUnderMouse);
    QTreeView::mouseDoubleClickEvent(ev);
}

void BreakWindow::contextMenuEvent(QContextMenuEvent *ev)
{
    QMenu menu;
    QItemSelectionModel *sm = selectionModel();
    QTC_ASSERT(sm, return);
    QModelIndexList si = sm->selectedIndexes();
    QModelIndex indexUnderMouse = indexAt(ev->pos());
    if (si.isEmpty() && indexUnderMouse.isValid())
        si.append(indexUnderMouse.sibling(indexUnderMouse.row(), 0));
    si = normalizeIndexes(si);

    const int rowCount = model()->rowCount();
    const unsigned engineCapabilities =
        model()->data(QModelIndex(), EngineCapabilitiesRole).toUInt();

    QAction *deleteAction = new QAction(tr("Delete Breakpoint"), &menu);
    deleteAction->setEnabled(si.size() > 0);

    QAction *deleteAllAction = new QAction(tr("Delete All Breakpoints"), &menu);
    deleteAllAction->setEnabled(si.size() > 0);

    // Delete by file: Find indices of breakpoints of the same file.
    QAction *deleteByFileAction = 0;
    QList<int> breakPointsOfFile;
    if (indexUnderMouse.isValid()) {
        const QModelIndex index = indexUnderMouse.sibling(indexUnderMouse.row(), 2);
        const QString file = model()->data(index).toString();
        if (!file.isEmpty()) {
            for (int i = 0; i < rowCount; i++)
                if (model()->data(model()->index(i, 2)).toString() == file)
                    breakPointsOfFile.push_back(i);
            if (breakPointsOfFile.size() > 1) {
                deleteByFileAction =
                    new QAction(tr("Delete Breakpoints of \"%1\"").arg(file), &menu);
                deleteByFileAction->setEnabled(true);
            }
        }
    }
    if (!deleteByFileAction) {
        deleteByFileAction = new QAction(tr("Delete Breakpoints of File"), &menu);
        deleteByFileAction->setEnabled(false);
    }

    QAction *adjustColumnAction =
        new QAction(tr("Adjust Column Widths to Contents"), &menu);

    QAction *alwaysAdjustAction =
        new QAction(tr("Always Adjust Column Widths to Contents"), &menu);

    alwaysAdjustAction->setCheckable(true);
    alwaysAdjustAction->setChecked(m_alwaysResizeColumnsToContents);

    QAction *editBreakpointAction =
        new QAction(tr("Edit Breakpoint..."), &menu);
    editBreakpointAction->setEnabled(si.size() > 0);

    int threadId = model()->data(QModelIndex(), CurrentThreadIdRole).toInt();
    QString associateTitle = threadId == -1
        ?  tr("Associate Breakpoint With All Threads")
        :  tr("Associate Breakpoint With Thread %1").arg(threadId);
    QAction *associateBreakpointAction = new QAction(associateTitle, &menu);
    associateBreakpointAction->setEnabled(si.size() > 0);

    QAction *synchronizeAction =
        new QAction(tr("Synchronize Breakpoints"), &menu);
    synchronizeAction->setEnabled(
        model()->data(QModelIndex(), EngineActionsEnabledRole).toBool());

    QModelIndex idx0 = (si.size() ? si.front() : QModelIndex());
    QModelIndex idx2 = idx0.sibling(idx0.row(), 2);
    bool enabled = si.isEmpty()
        || idx0.data(BreakpointEnabledRole).toBool();

    const QString str5 = si.size() > 1
        ? enabled
            ? tr("Disable Selected Breakpoints")
            : tr("Enable Selected Breakpoints")
        : enabled
            ? tr("Disable Breakpoint")
            : tr("Enable Breakpoint");
    QAction *toggleEnabledAction = new QAction(str5, &menu);
    toggleEnabledAction->setEnabled(si.size() > 0);

    const bool fullpath = si.isEmpty()
        || idx2.data(BreakpointEnabledRole).toBool();
    const QString str6 = fullpath ? tr("Use Short Path") : tr("Use Full Path");
    QAction *pathAction = new QAction(str6, &menu);
    pathAction->setEnabled(si.size() > 0);

    QAction *breakAtFunctionAction =
        new QAction(tr("Set Breakpoint at Function..."), this);
    QAction *breakAtMainAction =
        new QAction(tr("Set Breakpoint at Function \"main\""), this);
    QAction *breakAtThrowAction =
        new QAction(tr("Set Breakpoint at \"throw\""), this);
    QAction *breakAtCatchAction =
        new QAction(tr("Set Breakpoint at \"catch\""), this);

    menu.addAction(deleteAction);
    menu.addAction(editBreakpointAction);
    menu.addAction(associateBreakpointAction);
    menu.addAction(toggleEnabledAction);
    menu.addAction(pathAction);
    menu.addSeparator();
    menu.addAction(deleteAllAction);
    menu.addAction(deleteByFileAction);
    menu.addSeparator();
    menu.addAction(synchronizeAction);
    menu.addSeparator();
    menu.addAction(breakAtFunctionAction);
    menu.addAction(breakAtMainAction);
    if (engineCapabilities & BreakOnThrowAndCatchCapability) {
        menu.addAction(breakAtThrowAction);
        menu.addAction(breakAtCatchAction);
    }
    menu.addSeparator();
    menu.addAction(theDebuggerAction(UseToolTipsInBreakpointsView));
    menu.addAction(theDebuggerAction(UseAddressInBreakpointsView));
    menu.addAction(adjustColumnAction);
    menu.addAction(alwaysAdjustAction);
    menu.addSeparator();
    menu.addAction(theDebuggerAction(SettingsDialog));

    QAction *act = menu.exec(ev->globalPos());

    if (act == deleteAction) {
        deleteBreakpoints(si);
    } else if (act == deleteAllAction) {
        QList<int> allRows;
        for (int i = 0; i < rowCount; i++)
            allRows.push_back(i);
        deleteBreakpoints(allRows);
    }  else if (act == deleteByFileAction)
        deleteBreakpoints(breakPointsOfFile);
    else if (act == adjustColumnAction)
        resizeColumnsToContents();
    else if (act == alwaysAdjustAction)
        setAlwaysResizeColumnsToContents(!m_alwaysResizeColumnsToContents);
    else if (act == editBreakpointAction)
        editBreakpoint(si);
    else if (act == associateBreakpointAction)
        associateBreakpoint(si, threadId);
    else if (act == synchronizeAction)
        setModelData(RequestSynchronizeBreakpointsRole);
    else if (act == toggleEnabledAction)
        setBreakpointsEnabled(si, !enabled);
    else if (act == pathAction)
        setBreakpointsFullPath(si, !fullpath);
    else if (act == breakAtFunctionAction) {
        BreakByFunctionDialog dlg(this);
        if (dlg.exec())
            setModelData(RequestBreakByFunctionRole, dlg.functionName());
    } else if (act == breakAtMainAction)
        setModelData(RequestBreakByFunctionMainRole);
     else if (act == breakAtThrowAction)
        setModelData(RequestBreakByFunctionRole, "__cxa_throw");
     else if (act == breakAtCatchAction)
        setModelData(RequestBreakByFunctionRole, "__cxa_begin_catch");
}

void BreakWindow::setBreakpointsEnabled(const QModelIndexList &list, bool enabled)
{
    foreach (const QModelIndex &index, list)
        setModelData(BreakpointEnabledRole, enabled, index);
    setModelData(RequestSynchronizeBreakpointsRole);
}

void BreakWindow::setBreakpointsFullPath(const QModelIndexList &list, bool fullpath)
{
    foreach (const QModelIndex &index, list)
        setModelData(BreakpointUseFullPathRole, fullpath, index);
    setModelData(RequestSynchronizeBreakpointsRole);
}

void BreakWindow::deleteBreakpoints(const QModelIndexList &indexes)
{
    QTC_ASSERT(!indexes.isEmpty(), return);
    QList<int> list;
    foreach (const QModelIndex &index, indexes)
        list.append(index.row());
    deleteBreakpoints(list);
}

void BreakWindow::deleteBreakpoints(QList<int> list)
{
    if (list.empty())
        return;
    const int firstRow = list.front();
    qSort(list.begin(), list.end());
    for (int i = list.size(); --i >= 0; )
        setModelData(RequestRemoveBreakpointByIndexRole, list.at(i));

    const int row = qMin(firstRow, model()->rowCount() - 1);
    if (row >= 0)
        setCurrentIndex(model()->index(row, 0));
    setModelData(RequestSynchronizeBreakpointsRole);
}

void BreakWindow::editBreakpoint(const QModelIndexList &list)
{
    QDialog dlg(this);
    Ui::BreakCondition ui;
    ui.setupUi(&dlg);

    QTC_ASSERT(!list.isEmpty(), return);
    QModelIndex idx = list.front();
    const int row = idx.row();
    dlg.setWindowTitle(tr("Conditions on Breakpoint %1").arg(row));
    ui.lineEditFunction->hide();
    ui.labelFunction->hide();
    ui.lineEditFileName->hide();
    ui.labelFileName->hide();
    ui.lineEditLineNumber->hide();
    ui.labelLineNumber->hide();
    QAbstractItemModel *m = model();
    ui.lineEditCondition->setText(
        m->data(idx, BreakpointConditionRole).toString());
    ui.lineEditIgnoreCount->setText(
        m->data(idx, BreakpointIgnoreCountRole).toString());
    ui.lineEditThreadSpec->setText(
        m->data(idx, BreakpointThreadSpecRole).toString());

    if (dlg.exec() == QDialog::Rejected)
        return;

    foreach (const QModelIndex &idx, list) {
        //m->setData(idx.sibling(idx.row(), 1), ui.lineEditFunction->text());
        //m->setData(idx.sibling(idx.row(), 2), ui.lineEditFileName->text());
        //m->setData(idx.sibling(idx.row(), 3), ui.lineEditLineNumber->text());
        m->setData(idx, ui.lineEditCondition->text(), BreakpointConditionRole);
        m->setData(idx, ui.lineEditIgnoreCount->text(), BreakpointIgnoreCountRole);
        m->setData(idx, ui.lineEditThreadSpec->text(), BreakpointThreadSpecRole);
    }
    setModelData(RequestSynchronizeBreakpointsRole);
}

void BreakWindow::associateBreakpoint(const QModelIndexList &list, int threadId)
{
    QString str;
    if (threadId != -1)
        str = QString::number(threadId);
    foreach (const QModelIndex &index, list)
        setModelData(BreakpointThreadSpecRole, str, index);
    setModelData(RequestSynchronizeBreakpointsRole);
}

void BreakWindow::resizeColumnsToContents()
{
    for (int i = model()->columnCount(); --i >= 0; )
        resizeColumnToContents(i);
}

void BreakWindow::setAlwaysResizeColumnsToContents(bool on)
{
    m_alwaysResizeColumnsToContents = on;
    QHeaderView::ResizeMode mode = on
        ? QHeaderView::ResizeToContents : QHeaderView::Interactive;
    for (int i = model()->columnCount(); --i >= 0; )
        header()->setResizeMode(i, mode);
}

void BreakWindow::rowActivated(const QModelIndex &index)
{
    setModelData(RequestActivateBreakpointRole, index.row());
}

void BreakWindow::setModelData
    (int role, const QVariant &value, const QModelIndex &index)
{
    QTC_ASSERT(model(), return);
    model()->setData(index, value, role);
}


} // namespace Internal
} // namespace Debugger
