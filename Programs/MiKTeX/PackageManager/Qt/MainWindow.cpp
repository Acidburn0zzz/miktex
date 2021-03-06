/* MainWindow.cpp:

   Copyright (C) 2008-2018 Christian Schenk

   This file is part of MiKTeX Package Manager.

   MiKTeX Package Manager is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2, or
   (at your option) any later version.

   MiKTeX Package Manager is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with MiKTeX Package Manager; if not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA. */

#if defined(__MINGW32__)
#define _WIN32_WINNT 0x0600
#endif

#include <QtWidgets>
#include <QProgressDialog>

#include <memory>
#include <thread>

#include "mpm-version.h"

#include <miktex/Core/Debug>
#include <miktex/Core/Exceptions>
#include <miktex/Core/Paths>
#include <miktex/UI/Qt/ErrorDialog>
#include <miktex/UI/Qt/PackageInfoDialog>
#include <miktex/UI/Qt/SiteWizSheet>
#include <miktex/UI/Qt/UpdateDialog>

#if defined(MIKTEX_WINDOWS)
#include <miktex/Core/win/WindowsVersion.h>
#include <commctrl.h>
#endif

#include "MainWindow.h"
#include "PackageProxyModel.h"
#include "PackageTableModel.h"

using namespace MiKTeX::Core;
using namespace MiKTeX::Packages;
using namespace MiKTeX::UI::Qt;
using namespace std;

void MainWindow::SetupFilterToolBar()
{
  toolBarFilter = new QToolBar(this);
  toolBarFilter->setObjectName(QStringLiteral("fiterToolBar"));
  addToolBar(Qt::TopToolBarArea, toolBarFilter);
  lineEditFilter = new QLineEdit(toolBarFilter);
  lineEditFilter->setClearButtonEnabled(true);
  toolBarFilter->addWidget(lineEditFilter);
  toolBarFilter->addAction(actionFilter);
  connect(lineEditFilter, SIGNAL(returnPressed()), this, SLOT(Filter()));
}

void MainWindow::SetupContextMenu()
{
  contextMenu = new QMenu(treeView);
  treeView->setContextMenuPolicy(Qt::ActionsContextMenu);
  treeView->addAction(actionInstall);
  treeView->addAction(actionUninstall);
  treeView->addAction(actionProperties);
}

MainWindow::MainWindow() :
  packageManager(PackageManager::Create())
{
  setupUi(this);
  SetupFilterToolBar();
  SetupContextMenu();
  if (session->IsAdminMode())
  {
    setWindowTitle(windowTitle() + " (Admin)");
  }
  model = new PackageTableModel(packageManager, this);
  proxyModel = new PackageProxyModel(this);
  proxyModel->setSourceModel(model);
  proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
  treeView->setModel(proxyModel);
  treeView->sortByColumn(0, Qt::AscendingOrder);

  connect(treeView->selectionModel(),
    SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
    this,
    SLOT(EnableActions()));
  connect(actionProperties,
    SIGNAL(triggered()),
    this,
    SLOT(PropertyDialog()));
  connect(actionSelectInstallablePackages,
    SIGNAL(triggered()),
    this,
    SLOT(SelectInstallablePackages()));
  connect(actionInstall,
    SIGNAL(triggered()),
    this,
    SLOT(Install()));
  connect(actionUninstall,
    SIGNAL(triggered()),
    this,
    SLOT(Uninstall()));
  connect(actionChangeRepository,
    SIGNAL(triggered()),
    this,
    SLOT(RepositoryWizard()));
  connect(actionUpdateWizard,
    SIGNAL(triggered()),
    this,
    SLOT(UpdateWizard()));
  connect(actionSynchronize,
    SIGNAL(triggered()),
    this,
    SLOT(Synchronize()));
  connect(actionAbout,
    SIGNAL(triggered()),
    this,
    SLOT(AboutDialog()));
  connect(actionFilter,
    SIGNAL(triggered()),
    this,
    SLOT(Filter()));

  EnableActions();
}

void MainWindow::EnableActions()
{
  try
  {
    QModelIndexList selectedRows = treeView->selectionModel()->selectedRows();
    actionProperties->setEnabled(selectedRows.count() == 1);
    bool enableInstall = (selectedRows.count() > 0);
    bool enableUninstall = (selectedRows.count() > 0);
    if (session->IsMiKTeXDirect())
    {
      enableInstall = false;
      enableUninstall = false;
    }
    for (QModelIndexList::const_iterator it = selectedRows.begin(); it != selectedRows.end() && (enableInstall || enableUninstall); ++it)
    {
      PackageInfo packageInfo;
      if (!model->TryGetPackageInfo(proxyModel->mapToSource(*it), packageInfo))
      {
        MIKTEX_UNEXPECTED();
      }
      if (packageInfo.timeInstalled > 0)
      {
        enableInstall = false;
        enableUninstall = (enableUninstall && packageInfo.isRemovable);
      }
      else
      {
        enableUninstall = false;
      }
    }
    actionInstall->setEnabled(enableInstall);
    actionUninstall->setEnabled(enableUninstall);
  }
  catch (const MiKTeXException& e)
  {
    ErrorDialog::DoModal(this, e);
  }
  catch (const exception& e)
  {
    ErrorDialog::DoModal(this, e);
  }
}

void MainWindow::PropertyDialog()
{
  try
  {
    for (const QModelIndex& ind : treeView->selectionModel()->selectedRows())
    {
      PackageInfo packageInfo;
      if (!model->TryGetPackageInfo(proxyModel->mapToSource(ind), packageInfo))
      {
        MIKTEX_UNEXPECTED();
      }
      PackageInfoDialog::DoModal(this, packageInfo);
    }
  }
  catch (const MiKTeXException& e)
  {
    ErrorDialog::DoModal(this, e);
  }
  catch (const exception& e)
  {
    ErrorDialog::DoModal(this, e);
  }
}

void MainWindow::SelectInstallablePackages()
{
  try
  {
    QItemSelection selection;
    for (const auto& p : model->GetData())
    {
      if (p.second.timeInstalled == 0)
      {
        selection.append(QItemSelectionRange(proxyModel->mapFromSource(model->index(p.first, 0))));
      }
    }
    treeView->selectionModel()->select(selection, QItemSelectionModel::Select | QItemSelectionModel::Rows);
  }
  catch (const MiKTeXException& e)
  {
    ErrorDialog::DoModal(this, e);
  }
  catch (const exception& e)
  {
    ErrorDialog::DoModal(this, e);
  }
}

void MainWindow::Install()
{
  try
  {
    vector<string> toBeInstalled;
    vector<string> toBeRemoved;
    for (const QModelIndex& ind : treeView->selectionModel()->selectedRows())
    {
      PackageInfo packageInfo;
      if (!model->TryGetPackageInfo(proxyModel->mapToSource(ind), packageInfo))
      {
        MIKTEX_UNEXPECTED();
      }
      else if (packageInfo.timeInstalled == 0)
      {
        toBeInstalled.push_back(packageInfo.deploymentName);
      }
      else
      {
        toBeRemoved.push_back(packageInfo.deploymentName);
      }
    }
    QString message =
      tr("Your MiKTeX installation will now be updated:\n\n")
      + tr("%n package(s) will be installed\n", "", toBeInstalled.size())
      + tr("%n package(s) will be removed", "", toBeRemoved.size());
    if (QMessageBox::Ok != QMessageBox::information(this, "MiKTeX Package Manager", message, QMessageBox::Ok | QMessageBox::Cancel))
    {
      return;
    }

    int ret = UpdateDialog::DoModal(this, packageManager, toBeInstalled, toBeRemoved);
    if (ret == QDialog::Accepted)
    {
      model->Reload();
      treeView->update();
    }
  }
  catch (const MiKTeXException& e)
  {
    ErrorDialog::DoModal(this, e);
  }
  catch (const exception& e)
  {
    ErrorDialog::DoModal(this, e);
  }
}

void MainWindow::Uninstall()
{
  Install();
}

void MainWindow::RepositoryWizard()
{
  try
  {
    if (SiteWizSheet::DoModal(this) == QDialog::Accepted)
    {
      emit Synchronize();
    }
  }
  catch (const MiKTeXException& e)
  {
    ErrorDialog::DoModal(this, e);
  }
  catch (const exception& e)
  {
    ErrorDialog::DoModal(this, e);
  }
}

void MainWindow::Synchronize()
{
  try
  {
    unique_ptr<PackageInstaller> pInstaller(packageManager->CreateInstaller());
    pInstaller->UpdateDbAsync();
    int numSteps = 10;
    QProgressDialog progress(tr("Synchronizing the package database..."), tr("Cancel"), 0, numSteps, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(1000);
    for (int step = 0; !progress.wasCanceled(); ++step)
    {
      if (step < numSteps)
      {
        progress.setValue(step);
      }
      PackageInstaller::ProgressInfo progressinfo = pInstaller->GetProgressInfo();
      if (progressinfo.ready)
      {
        break;
      }
      this_thread::sleep_for(chrono::milliseconds(1000));
    }
    pInstaller->Dispose();
    model->Reload();
    treeView->update();
    if (!progress.wasCanceled())
    {
      progress.setValue(numSteps);
    }
  }
  catch (const MiKTeXException& e)
  {
    ErrorDialog::DoModal(this, e);
  }
  catch (const exception& e)
  {
    ErrorDialog::DoModal(this, e);
  }
}

void MainWindow::AboutDialog()
{
  QString message;
  message = tr("MiKTeX Package Manager");
  message += " ";
  message += MIKTEX_COMPONENT_VERSION_STR;
  message += "\n\n";
  message += tr("MiKTeX Package Manager is free software. You are welcome to redistribute it under certain conditions. See the help file for more information.\n\nMiKTeX Package Manager comes WITH ABSOLUTELY NO WARRANTY OF ANY KIND.");
  QMessageBox::about(this, tr("MiKTeX Package Manager"), message);
}

void MainWindow::Filter()
{
  proxyModel->SetFilter(lineEditFilter->text().toUtf8().constData());
}

void MainWindow::ContextMenu(const QPoint& point)
{
  QModelIndex index = treeView->indexAt(point);
  if (index.isValid())
  {
    contextMenu->exec(treeView->mapToGlobal(point));
  }
}

void MainWindow::UpdateWizard()
{
  try
  {
    if (!session->UnloadFilenameDatabase())
    {
      MIKTEX_UNEXPECTED();
    }
    string console = session->IsAdminMode() ? MIKTEX_CONSOLE_ADMIN_EXE : MIKTEX_CONSOLE_EXE;
    PathName exePath = session->GetSpecialPath(SpecialPath::InternalBinDirectory);
    exePath /= console;
    vector<string> args = { console, "--start-page", "updates" };
    if (session->IsAdminMode())
    {
      args.push_back("--admin");
    }
    Process::Start(exePath, args);
  }
  catch (const MiKTeXException& e)
  {
    ErrorDialog::DoModal(this, e);
  }
  catch (const exception& e)
  {
    ErrorDialog::DoModal(this, e);
  }
}
