/****************************************************************************
**
** Copyright (C) 2019 BlackINT3
** Contact: https://github.com/BlackINT3/OpenArk
**
** GNU Lesser General Public License Usage (LGPL)
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
****************************************************************************/
#include "storage.h"
#include "../../../OpenArkDrv/arkdrv-api/arkdrv-api.h"

bool UnlockFileSortFilterProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
	auto s1 = sourceModel()->data(left); auto s2 = sourceModel()->data(right);
	return QString::compare(s1.toString(), s2.toString(), Qt::CaseInsensitive) < 0;
}


KernelStorage::KernelStorage()
{

}

KernelStorage::~KernelStorage()
{

}

void KernelStorage::onTabChanged(int index)
{
	CommonTabObject::onTabChanged(index);
}

bool KernelStorage::eventFilter(QObject *obj, QEvent *e)
{
	if (e->type() == QEvent::ContextMenu) {
		QMenu *menu = nullptr;
		if (obj == ui_->unlockView->viewport()) menu = unlock_menu_;
		if (obj == ui_->fsfltView->viewport()) menu = fsflt_menu_;
		QContextMenuEvent *ctxevt = dynamic_cast<QContextMenuEvent*>(e);
		if (ctxevt && menu) {
			menu->move(ctxevt->globalPos());
			menu->show();
		}
	}
	return QWidget::eventFilter(obj, e);
}

void KernelStorage::ModuleInit(Ui::Kernel *ui, Kernel *kernel)
{
	this->ui_ = ui;

	Init(ui->tabStorage, TAB_KERNEL, TAB_KERNEL_STORAGE);

	InitFileUnlockView();
	InitFileFilterView();
}

ULONG64 GetFreeLibraryAddress(DWORD pid); //temp code

void KernelStorage::InitFileUnlockView()
{
	unlock_model_ = new QStandardItemModel;
	QTreeView *view = ui_->unlockView;
	proxy_unlock_ = new UnlockFileSortFilterProxyModel(view);
	proxy_unlock_->setSourceModel(unlock_model_);
	proxy_unlock_->setDynamicSortFilter(true);
	proxy_unlock_->setFilterKeyColumn(1);
	view->setModel(proxy_unlock_);
	view->selectionModel()->setModel(proxy_unlock_);
	view->header()->setSortIndicator(-1, Qt::AscendingOrder);
	view->setSortingEnabled(true);
	view->viewport()->installEventFilter(this);
	view->installEventFilter(this);
	view->setEditTriggers(QAbstractItemView::NoEditTriggers);
	std::pair<int, QString> colum_layout[] = {
		{ 150, tr("ProcessName") },
		{ 50, tr("PID") },
		{ 240, tr("FilePath") },
		{ 240, tr("ProcessPath") },
		{ 150, tr("FileObject") },
		{ 100, tr("FileHandle") },
	};
	QStringList name_list;
	for (auto p : colum_layout)  name_list << p.second;
	unlock_model_->setHorizontalHeaderLabels(name_list);
	for (int i = 0; i < _countof(colum_layout); i++) {
		view->setColumnWidth(i, colum_layout[i].first);
	}
	unlock_menu_ = new QMenu();
	unlock_menu_->addAction(tr("Refresh"), this, [&] {
		ui_->showHoldBtn->click();
	});

	unlock_menu_->addAction(tr("Copy"), this, [&] {
		auto view = ui_->unlockView;
		ClipboardCopyData(GetCurItemViewData(view, GetCurViewColumn(view)).toStdString());
	});

	unlock_menu_->addAction(tr("CloseHandle"), this, [&] {
		ui_->unlockFileBtn->click();
	});

	unlock_menu_->addAction(tr("CloseAllHandle"), this, [&] {
		ui_->unlockFileAllBtn->click();
	});

	unlock_menu_->addAction(tr("KillProcess"), this, [&] {
		ui_->killProcessBtn->click();
	});

	unlock_menu_->addAction(tr("LocateFile"), this, [&] {
		
	});

	unlock_menu_->addAction(tr("ScanFile"), this, [&] {

	});

	unlock_menu_->addAction(tr("ViewFileAttribute"), this, [&] {

	});


	connect(ui_->showHoldBtn, &QPushButton::clicked, [&] {
		DISABLE_RECOVER();
		ClearItemModelData(unlock_model_, 0);

		QString file = ui_->inputPathEdit->text();
		if (file.length() <= 0) {
			INFO(L"Please input the file path...");
			return;
		}
		std::wstring path;
		std::vector<HANDLE_ITEM> items;
		UNONE::ObParseToNtPathW(file.toStdWString(), path);
		UNONE::StrLowerW(path);
		ArkDrvApi::Storage::UnlockEnum(path, items);
		for (auto item : items) {
			auto pid = (DWORD)item.pid;
			auto &&ppath = UNONE::PsGetProcessPathW(pid);
			auto &&pname = UNONE::FsPathToNameW(ppath);
			std::wstring fpath;
			UNONE::ObParseToDosPathW(item.name, fpath);
			auto item_pname = new QStandardItem(LoadIcon(WStrToQ(ppath)), WStrToQ(pname));
			auto item_pid = new QStandardItem(WStrToQ(UNONE::StrFormatW(L"%d", pid)));
			auto item_fpath = new QStandardItem(WStrToQ(fpath));
			auto item_fobj = new QStandardItem(WStrToQ(UNONE::StrFormatW(L"0x%p", item.object)));
			auto item_ppath = new QStandardItem(WStrToQ(ppath));
			auto item_fhandle = new QStandardItem(WStrToQ(UNONE::StrFormatW(L"0x%08X", (DWORD)item.handle)));
			
			auto count = unlock_model_->rowCount();
			unlock_model_->setItem(count, 0, item_pname);
			unlock_model_->setItem(count, 1, item_pid);
			unlock_model_->setItem(count, 2, item_fpath);
			unlock_model_->setItem(count, 3, item_ppath);
			unlock_model_->setItem(count, 4, item_fobj);
			unlock_model_->setItem(count, 5, item_fhandle);
		}
		// Add Process Dll Path
		std::vector<DWORD> pids;
		UNONE::PsGetAllProcess(pids);
		for (auto pid : pids) {
			UNONE::PsEnumModule(pid, [&](MODULEENTRY32W& entry)->bool {
				QString modname = WCharsToQ(entry.szModule);
				QString modpath = WCharsToQ(entry.szExePath);
				HANDLE  hmodule = entry.hModule;
				QString pname = WStrToQ(UNONE::PsGetProcessNameW(pid));
				QString ppath = WStrToQ(UNONE::PsGetProcessPathW(pid));
				auto index = modpath.toLower().indexOf(file.toLower());
				if (index >= 0) {
					auto item_pname = new QStandardItem(LoadIcon(ppath), pname);
					auto item_pid = new QStandardItem(WStrToQ(UNONE::StrFormatW(L"%d", pid)));
					auto item_fpath = new QStandardItem(modpath);
					auto item_fobj = new QStandardItem(WStrToQ(UNONE::StrFormatW(L"")));
					auto item_ppath = new QStandardItem(ppath);
					auto item_fhandle = new QStandardItem(WStrToQ(UNONE::StrFormatW(L"0x%p", hmodule)));

					auto count = unlock_model_->rowCount();
					unlock_model_->setItem(count, 0, item_pname);
					unlock_model_->setItem(count, 1, item_pid);
					unlock_model_->setItem(count, 2, item_fpath);
					unlock_model_->setItem(count, 3, item_ppath);
					unlock_model_->setItem(count, 4, item_fobj);
					unlock_model_->setItem(count, 5, item_fhandle);
					INFO(entry.szExePath);
				}
				return true;
			});
		}
	});

	connect(ui_->unlockFileBtn, &QPushButton::clicked, [&]{
		INFO(L"Click the unlockFileBtn button");
		DISABLE_RECOVER();
		auto selected = ui_->unlockView->selectionModel()->selectedIndexes();
		if (selected.empty()) {
			WARN(L"Not select the item of unlock file!");
			return;
		}
		for (int i = 0; i < selected.size() / 6; i++) {
			auto pname = ui_->unlockView->model()->itemData(selected[i * 6]).values()[0].toString();
			auto pid = ui_->unlockView->model()->itemData(selected[i * 6 + 1]).values()[0].toUInt();
			auto fpath = ui_->unlockView->model()->itemData(selected[i * 6 + 2]).values()[0].toString();
			auto ppath = ui_->unlockView->model()->itemData(selected[i * 6 + 3]).values()[0].toString();
			auto fobj = ui_->unlockView->model()->itemData(selected[i * 6 + 4]).values()[0].toString();
			auto fhandle = ui_->unlockView->model()->itemData(selected[i * 6 + 5]).values()[0].toString();
			if (fobj.length() > 0 ) {
				// close by driver
				HANDLE_ITEM handle_item = { 0 };
				handle_item.pid = HANDLE(pid);
				QString qshandle = fhandle.replace(QRegExp("0x"), "");
				handle_item.handle = HANDLE(UNONE::StrToHexA(qshandle.toStdString().c_str()));
				ArkDrvApi::Storage::UnlockClose(handle_item);
				INFO(L"Unlock file handle(pid: %i, handle: %p)", pid, handle_item.handle);
			}
			else {
				// close by freelibrary
				ULONG64 remote_routine = GetFreeLibraryAddress(pid);
				if (remote_routine) {
					QString qshandle = fhandle.replace(QRegExp("0x"), "");
					ULONG64 pararm = UNONE::StrToHexA(qshandle.toStdString().c_str());
					UNONE::PsCreateRemoteThread((DWORD)pid, remote_routine, pararm, 0);
				}
			}
			
		}
		ui_->showHoldBtn->click();
		INFO(L"Reflush the unlock file handle");
	});

	connect(ui_->unlockFileAllBtn, &QPushButton::clicked, [&] {
		DISABLE_RECOVER();
		for (int i = 0; i < unlock_model_->rowCount(); i++) {
			QStandardItem *item = unlock_model_->item(i, 1); //pid
			auto pid = item->text().toUInt();

			item = unlock_model_->item(i, 5); //handle
			auto handle = HANDLE(UNONE::StrToHexA(item->text().replace(QRegExp("0x"), "").toStdString().c_str()));
			HANDLE_ITEM handle_item = { 0 };
			handle_item.pid = HANDLE(pid);
			handle_item.handle = handle;
			ArkDrvApi::Storage::UnlockClose(handle_item);
			INFO(L"Unlock file handle all(pid: %i, handle: %p)", pid, handle_item.handle);
		}
		ui_->showHoldBtn->click();
		INFO(L"Reflush the unlock file handle");

	});

	connect(ui_->killProcessBtn, &QPushButton::clicked, [&] {
		INFO(L"Click the killProcessBtn button");
		DISABLE_RECOVER();
		auto selected = ui_->unlockView->selectionModel()->selectedIndexes();
		if (selected.empty()) {
			WARN(L"Not select the item of kill process!");
			return;
		}
		for (int i = 0; i < selected.size() / 6; i++) {
			auto pid = ui_->unlockView->model()->itemData(selected[i * 6 + 1]).values()[0].toUInt();
			UNONE::PsKillProcess(pid);
			INFO(L"Kill process(pid: %i)", pid);
		}
		ui_->showHoldBtn->click();
		INFO(L"Reflush the unlock file handle");
	});
}

void KernelStorage::InitFileFilterView()
{
	fsflt_model_ = new QStandardItemModel;
	fsflt_model_->setHorizontalHeaderLabels(QStringList() << tr("Name") << tr("Value"));
	SetDefaultTreeViewStyle(ui_->fsfltView, fsflt_model_);
	ui_->fsfltView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
	ui_->fsfltView->viewport()->installEventFilter(this);
	ui_->fsfltView->installEventFilter(this);
	fsflt_menu_ = new QMenu();
	fsflt_menu_->addAction(tr("ExpandAll"), this, SLOT(onExpandAll()));
}

void KernelStorage::ShowUnlockFiles()
{

}

// temp function 
ULONG64 GetFreeLibraryAddress32(DWORD pid)
{
	ULONG64 addr = 0;
#ifdef _AMD64_
	std::vector<UNONE::MODULE_BASE_INFOW> mods;
	UNONE::PsGetModulesInfoW(pid, mods);
	auto it = std::find_if(std::begin(mods), std::end(mods), [](UNONE::MODULE_BASE_INFOW &info) {
		return UNONE::StrCompareIW(info.BaseDllName, L"kernel32.dll");
	});
	if (it == std::end(mods)) {
		UNONE_ERROR("not found kernel32.dll");
		return NULL;
	}
	ULONG64 base = it->DllBase;
	auto &&path = UNONE::OsSyswow64DirW() + L"\\kernel32.dll";
	auto image = UNONE::PeMapImageByPathW(path);
	if (!image) {
		UNONE_ERROR("MapImage %s failed, err:%d", path.c_str(), GetLastError());
		return NULL;
	}
	auto pFreeLibrary = UNONE::PeGetProcAddress(image, "FreeLibrary");
	UNONE::PeUnmapImage(image);
	if (pFreeLibrary == NULL) {
		UNONE_ERROR("PsGetProcAddress err:%d", GetLastError());
		return NULL;
	}
	addr = (ULONG64)pFreeLibrary - (ULONG64)image + base;
#else
	addr = (ULONG64)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "FreeLibrary");
#endif
	return addr;
}

ULONG64 GetFreeLibraryAddress(DWORD pid)
{
	ULONG64 addr = 0;
	if (UNONE::PsIsX64(pid)) {
		addr = (ULONG64)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "FreeLibrary");
	}
	else {
		addr = GetFreeLibraryAddress32(pid);
	}
	return addr;
}
