/*
 * Copyright (C) 2012 - 2015  Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#include "filelauncher.h"
#include "applaunchcontext.h"
#include <QMessageBox>
#include <QDebug>
#include "execfiledialog_p.h"
#include "appchooserdialog.h"
#include "utilities.h"

#include "core/fileinfojob.h"
#include "mountoperation.h"

namespace Fm {

FileLauncher::FileLauncher() {
    resetExecActions();
}

FileLauncher::~FileLauncher() {
}


bool FileLauncher::launchFiles(QWidget* parent, const FileInfoList &file_infos) {
    resetExecActions();
    multiple_ = file_infos.size() > 1;
    GObjectPtr<FmAppLaunchContext> context{fm_app_launch_context_new_for_widget(parent), false};
    bool ret = BasicFileLauncher::launchFiles(file_infos, G_APP_LAUNCH_CONTEXT(context.get()));
    launchedFiles(file_infos);
    return ret;
}

bool FileLauncher::launchPaths(QWidget* parent, const FilePathList& paths) {
    resetExecActions();
    multiple_ = paths.size() > 1;
    GObjectPtr<FmAppLaunchContext> context{fm_app_launch_context_new_for_widget(parent), false};
    bool ret = BasicFileLauncher::launchPaths(paths, G_APP_LAUNCH_CONTEXT(context.get()));
    launchedPaths(paths);
    return ret;
}

bool FileLauncher::launchWithApp(QWidget* parent, GAppInfo* app, const FilePathList& paths) {
    GObjectPtr<FmAppLaunchContext> context{fm_app_launch_context_new_for_widget(parent), false};
    bool ret = BasicFileLauncher::launchWithApp(app, paths, G_APP_LAUNCH_CONTEXT(context.get()));
    launchedPaths(paths);
    return ret;
}

void FileLauncher::launchedFiles(const FileInfoList& /*files*/) const {
}

void FileLauncher::launchedPaths(const FilePathList& /*paths*/) const {
}

int FileLauncher::ask(const char* /*msg*/, char* const* /*btn_labels*/, int /*default_btn*/) {
    /* FIXME: set default button properly */
    // return fm_askv(data->parent, nullptr, msg, btn_labels);
    return -1;
}

GAppInfoPtr FileLauncher::chooseApp(const FileInfoList& /*fileInfos*/, const char *mimeType, GErrorPtr& /*err*/) {
    AppChooserDialog dlg(nullptr);
    GAppInfoPtr app;
    if(mimeType) {
        dlg.setMimeType(Fm::MimeType::fromName(mimeType));
    }
    else {
        dlg.setCanSetDefault(false);
    }
    // FIXME: show error properly?
    if(execModelessDialog(&dlg) == QDialog::Accepted) {
        app = dlg.selectedApp();
    }
    return app;
}

bool FileLauncher::openFolder(GAppLaunchContext *ctx, const FileInfoList &folderInfos, GErrorPtr &err) {
    return BasicFileLauncher::openFolder(ctx, folderInfos, err);
}

bool FileLauncher::showError(GAppLaunchContext* /*ctx*/, const GErrorPtr &err, const FilePath &path, const FileInfoPtr &info) {
    if(err == nullptr) {
        return false;
    }
    /* ask for mount if trying to launch unmounted path */
    if(err->domain == G_IO_ERROR) {
        if(path && err->code == G_IO_ERROR_NOT_MOUNTED) {
            MountOperation* op = new MountOperation(true);
            op->setAutoDestroy(true);
            if(info && info->isMountable()) {
                // this is a mountable shortcut (such as computer:///xxxx.drive)
                op->mountMountable(path);
            }
            else {
                op->mountEnclosingVolume(path);
            }
            if(op->wait()) {
                // if the mount operation succeeds, we can ignore the error and continue
                return true;
            }
        }
        else if(err->code == G_IO_ERROR_FAILED_HANDLED) {
            return true;    /* don't show error message */
        }
    }
    QMessageBox dlg(QMessageBox::Critical, QObject::tr("Error"), QString::fromUtf8(err->message), QMessageBox::Ok);
    execModelessDialog(&dlg);
    return false;
}

void FileLauncher::resetExecActions() {
    multiple_ = false;
    dekstopEntryAction_ = BasicFileLauncher::ExecAction::NONE;
    scriptAction_ = BasicFileLauncher::ExecAction::NONE;
    execAction_ = BasicFileLauncher::ExecAction::NONE;
}

BasicFileLauncher::ExecAction FileLauncher::askExecFile(const FileInfoPtr &file) {
    if(multiple_) {
        if(file->isDesktopEntry()) {
            if(dekstopEntryAction_ != BasicFileLauncher::ExecAction::NONE) {
                return dekstopEntryAction_;
            }
        }
        else if(file->isText()) {
            if(scriptAction_ != BasicFileLauncher::ExecAction::NONE) {
                return scriptAction_;
            }
        }
        else if(execAction_ != BasicFileLauncher::ExecAction::NONE) {
            return execAction_;
        }
    }

    ExecFileDialog dlg(*file);
    if(multiple_) {
        dlg.allowRemembering();
    }
    execModelessDialog(&dlg);
    auto res = dlg.result();
    if(dlg.isRemembered()) {
        if(file->isDesktopEntry()) {
            dekstopEntryAction_ = res;
        }
        else if(file->isText()) {
            scriptAction_ = res;
        }
        else {
            execAction_ = res;
        }
    }
    return res;
}


} // namespace Fm
