; *** Inno Setup version 6.5.0+ Indonesian messages ***
; Translated for VB Recorder. Based on Default.isl of Inno Setup 7.1.

[LangOptions]
LanguageName=Bahasa Indonesia
LanguageID=$0421
LanguageCodePage=1252

[Messages]

; *** Application titles
SetupAppTitle=Instalasi
SetupWindowTitle=Instalasi - %1
UninstallAppTitle=Hapus Instalasi
UninstallAppFullTitle=Hapus Instalasi %1

; *** Misc. common
InformationTitle=Informasi
ConfirmTitle=Konfirmasi
ErrorTitle=Kesalahan

; *** SetupLdr messages
SetupLdrStartupMessage=Program ini akan menginstal %1. Apakah Anda ingin melanjutkan?
LdrCannotCreateTemp=Tidak dapat membuat file sementara. Instalasi dibatalkan
LdrCannotExecTemp=Tidak dapat menjalankan file di direktori sementara. Instalasi dibatalkan
HelpTextNote=

; *** Startup error messages
LastErrorMessage=%1.%n%nKesalahan %2: %3
SetupFileMissing=File %1 tidak ada di direktori instalasi. Harap perbaiki masalah ini atau dapatkan salinan program yang baru.
SetupFileCorrupt=File instalasi rusak. Harap dapatkan salinan program yang baru.
SetupFileCorruptOrWrongVer=File instalasi rusak, atau tidak cocok dengan versi Instalasi ini. Harap perbaiki masalah ini atau dapatkan salinan program yang baru.
InvalidParameter=Parameter yang tidak valid diberikan pada baris perintah:%n%n%1
SetupAlreadyRunning=Instalasi sudah berjalan.
WindowsVersionNotSupported=Program ini tidak mendukung versi Windows yang berjalan di komputer Anda.
WindowsServicePackRequired=Program ini memerlukan %1 Service Pack %2 atau yang lebih baru.
NotOnThisPlatform=Program ini tidak akan berjalan di %1.
OnlyOnThisPlatform=Program ini harus dijalankan di %1.
OnlyOnTheseArchitectures=Program ini hanya dapat diinstal pada versi Windows yang dirancang untuk arsitektur prosesor berikut:%n%n%1
WinVersionTooLowError=Program ini memerlukan %1 versi %2 atau yang lebih baru.
WinVersionTooHighError=Program ini tidak dapat diinstal pada %1 versi %2 atau yang lebih baru.
AdminPrivilegesRequired=Anda harus masuk sebagai administrator saat menginstal program ini.
PowerUserPrivilegesRequired=Anda harus masuk sebagai administrator atau sebagai anggota grup Power Users saat menginstal program ini.
SetupAppRunningError=Instalasi mendeteksi bahwa %1 sedang berjalan.%n%nHarap tutup semua salinannya sekarang, lalu klik OK untuk melanjutkan, atau Batal untuk keluar.
UninstallAppRunningError=Hapus Instalasi mendeteksi bahwa %1 sedang berjalan.%n%nHarap tutup semua salinannya sekarang, lalu klik OK untuk melanjutkan, atau Batal untuk keluar.

; *** Startup questions
PrivilegesRequiredOverrideTitle=Pilih Mode Instalasi
PrivilegesRequiredOverrideInstruction=Pilih mode instalasi
PrivilegesRequiredOverrideText1=%1 dapat diinstal untuk semua pengguna (memerlukan hak administrator), atau hanya untuk Anda.
PrivilegesRequiredOverrideText2=%1 dapat diinstal hanya untuk Anda, atau untuk semua pengguna (memerlukan hak administrator).
PrivilegesRequiredOverrideAllUsers=Instal untuk &semua pengguna
PrivilegesRequiredOverrideAllUsersRecommended=Instal untuk &semua pengguna (disarankan)
PrivilegesRequiredOverrideCurrentUser=Instal hanya untuk &saya
PrivilegesRequiredOverrideCurrentUserRecommended=Instal hanya untuk &saya (disarankan)

; *** Misc. errors
ErrorCreatingDir=Instalasi tidak dapat membuat direktori "%1"
ErrorTooManyFilesInDir=Tidak dapat membuat file di direktori "%1" karena berisi terlalu banyak file

; *** Setup common messages
ExitSetupTitle=Keluar dari Instalasi
ExitSetupMessage=Instalasi belum selesai. Jika Anda keluar sekarang, program tidak akan diinstal.%n%nAnda dapat menjalankan Instalasi lagi di lain waktu untuk menyelesaikan instalasi.%n%nKeluar dari Instalasi?
AboutSetupMenuItem=&Tentang Instalasi...
AboutSetupTitle=Tentang Instalasi
AboutSetupMessage=%1 %2%n%3%n%nSitus web %1:%n%4
AboutSetupNote=
TranslatorNote=

; *** Buttons
ButtonBack=< &Kembali
ButtonNext=&Berikutnya >
ButtonInstall=&Instal
ButtonOK=OK
ButtonCancel=Batal
ButtonYes=&Ya
ButtonYesToAll=Ya untuk &Semua
ButtonNo=&Tidak
ButtonNoToAll=T&idak untuk Semua
ButtonFinish=&Selesai
ButtonBrowse=&Telusuri...
ButtonWizardBrowse=T&elusuri...
ButtonNewFolder=&Buat Folder Baru

; *** "Select Language" dialog messages
SelectLanguageTitle=Pilih Bahasa Instalasi
SelectLanguageLabel=Pilih bahasa yang digunakan selama instalasi.

; *** Common wizard text
ClickNext=Klik Berikutnya untuk melanjutkan, atau Batal untuk keluar dari Instalasi.
BeveledLabel=
BrowseDialogTitle=Telusuri Folder
BrowseDialogLabel=Pilih folder dalam daftar di bawah ini, lalu klik OK.
NewFolderName=Folder Baru

; *** "Welcome" wizard page
WelcomeLabel1=Selamat datang di Panduan Instalasi [name]
WelcomeLabel2=Program ini akan menginstal [name/ver] di komputer Anda.%n%nDisarankan untuk menutup semua aplikasi lain sebelum melanjutkan.

; *** "Password" wizard page
WizardPassword=Kata Sandi
PasswordLabel1=Instalasi ini dilindungi kata sandi.
PasswordLabel3=Harap masukkan kata sandi, lalu klik Berikutnya untuk melanjutkan. Kata sandi membedakan huruf besar dan kecil.
PasswordEditLabel=&Kata sandi:
IncorrectPassword=Kata sandi yang Anda masukkan salah. Silakan coba lagi.

; *** "License Agreement" wizard page
WizardLicense=Perjanjian Lisensi
LicenseLabel=Harap baca informasi penting berikut sebelum melanjutkan.
LicenseLabel3=Harap baca Perjanjian Lisensi berikut. Anda harus menerima ketentuan perjanjian ini sebelum melanjutkan instalasi.
LicenseAccepted=Saya &menerima perjanjian ini
LicenseNotAccepted=Saya &tidak menerima perjanjian ini

; *** "Information" wizard pages
WizardInfoBefore=Informasi
InfoBeforeLabel=Harap baca informasi penting berikut sebelum melanjutkan.
InfoBeforeClickLabel=Jika Anda siap melanjutkan Instalasi, klik Berikutnya.
WizardInfoAfter=Informasi
InfoAfterLabel=Harap baca informasi penting berikut sebelum melanjutkan.
InfoAfterClickLabel=Jika Anda siap melanjutkan Instalasi, klik Berikutnya.

; *** "User Information" wizard page
WizardUserInfo=Informasi Pengguna
UserInfoDesc=Harap masukkan informasi Anda.
UserInfoName=&Nama Pengguna:
UserInfoOrg=&Organisasi:
UserInfoSerial=Nomor &Seri:
UserInfoNameRequired=Anda harus memasukkan nama.

; *** "Select Destination Location" wizard page
WizardSelectDir=Pilih Lokasi Tujuan
SelectDirDesc=Di mana [name] akan diinstal?
SelectDirLabel3=Instalasi akan menginstal [name] ke folder berikut.
SelectDirBrowseLabel=Untuk melanjutkan, klik Berikutnya. Jika Anda ingin memilih folder lain, klik Telusuri.
DiskSpaceGBLabel=Diperlukan setidaknya [gb] GB ruang disk kosong.
DiskSpaceMBLabel=Diperlukan setidaknya [mb] MB ruang disk kosong.
CannotInstallToNetworkDrive=Instalasi tidak dapat menginstal ke drive jaringan.
CannotInstallToUNCPath=Instalasi tidak dapat menginstal ke jalur UNC.
InvalidPath=Anda harus memasukkan jalur lengkap dengan huruf drive; misalnya:%n%nC:\App%n%natau jalur UNC dalam bentuk:%n%n\\server\share
InvalidDrive=Drive atau share UNC yang Anda pilih tidak ada atau tidak dapat diakses. Harap pilih yang lain.
DiskSpaceWarningTitle=Ruang Disk Tidak Cukup
DiskSpaceWarning=Instalasi memerlukan setidaknya %1 KB ruang kosong untuk menginstal, tetapi drive yang dipilih hanya memiliki %2 KB.%n%nApakah Anda tetap ingin melanjutkan?
DirNameTooLong=Nama folder atau jalur terlalu panjang.
InvalidDirName=Nama folder tidak valid.
BadDirName32=Nama folder tidak boleh mengandung karakter berikut:%n%n%1
DirExistsTitle=Folder Sudah Ada
DirExists=Folder:%n%n%1%n%nsudah ada. Apakah Anda tetap ingin menginstal ke folder tersebut?
DirDoesntExistTitle=Folder Tidak Ada
DirDoesntExist=Folder:%n%n%1%n%ntidak ada. Apakah Anda ingin folder tersebut dibuat?

; *** "Select Components" wizard page
WizardSelectComponents=Pilih Komponen
SelectComponentsDesc=Komponen mana yang akan diinstal?
SelectComponentsLabel2=Pilih komponen yang ingin Anda instal; kosongkan komponen yang tidak ingin Anda instal. Klik Berikutnya jika Anda siap melanjutkan.
FullInstallation=Instalasi lengkap
CompactInstallation=Instalasi ringkas
CustomInstallation=Instalasi kustom
NoUninstallWarningTitle=Komponen Sudah Ada
NoUninstallWarning=Instalasi mendeteksi bahwa komponen berikut sudah terinstal di komputer Anda:%n%n%1%n%nMembatalkan pilihan komponen ini tidak akan menghapus instalasinya.%n%nApakah Anda tetap ingin melanjutkan?
ComponentSize1=%1 KB
ComponentSize2=%1 MB
ComponentsDiskSpaceGBLabel=Pilihan saat ini memerlukan setidaknya [gb] GB ruang disk.
ComponentsDiskSpaceMBLabel=Pilihan saat ini memerlukan setidaknya [mb] MB ruang disk.

; *** "Select Additional Tasks" wizard page
WizardSelectTasks=Pilih Tugas Tambahan
SelectTasksDesc=Tugas tambahan apa yang akan dilakukan?
SelectTasksLabel2=Pilih tugas tambahan yang ingin dilakukan oleh Instalasi saat menginstal [name], lalu klik Berikutnya.

; *** "Select Start Menu Folder" wizard page
WizardSelectProgramGroup=Pilih Folder Menu Start
SelectStartMenuFolderDesc=Di mana Instalasi akan menempatkan pintasan program?
SelectStartMenuFolderLabel3=Instalasi akan membuat pintasan program di folder Menu Start berikut.
SelectStartMenuFolderBrowseLabel=Untuk melanjutkan, klik Berikutnya. Jika Anda ingin memilih folder lain, klik Telusuri.
MustEnterGroupName=Anda harus memasukkan nama folder.
GroupNameTooLong=Nama folder atau jalur terlalu panjang.
InvalidGroupName=Nama folder tidak valid.
BadGroupName=Nama folder tidak boleh mengandung karakter berikut:%n%n%1
NoProgramGroupCheck2=&Jangan buat folder Menu Start

; *** "Ready to Install" wizard page
WizardReady=Siap Menginstal
ReadyLabel1=Instalasi sekarang siap untuk mulai menginstal [name] di komputer Anda.
ReadyLabel2a=Klik Instal untuk melanjutkan instalasi, atau klik Kembali jika Anda ingin meninjau atau mengubah pengaturan.
ReadyLabel2b=Klik Instal untuk melanjutkan instalasi.
ReadyMemoUserInfo=Informasi pengguna:
ReadyMemoDir=Lokasi tujuan:
ReadyMemoType=Jenis instalasi:
ReadyMemoComponents=Komponen yang dipilih:
ReadyMemoGroup=Folder Menu Start:
ReadyMemoTasks=Tugas tambahan:

; *** TDownloadWizardPage wizard page and DownloadTemporaryFile
DownloadingLabel2=Mengunduh file...
ButtonStopDownload=&Hentikan unduhan
StopDownload=Apakah Anda yakin ingin menghentikan unduhan?
ErrorDownloadAborted=Unduhan dibatalkan
ErrorDownloadFailed=Unduhan gagal: %1 %2
ErrorDownloadSizeFailed=Gagal mendapatkan ukuran: %1 %2
ErrorProgress=Kemajuan tidak valid: %1 dari %2
ErrorFileSize=Ukuran file tidak valid: diharapkan %1, ditemukan %2

; *** TExtractionWizardPage wizard page and ExtractArchive
ExtractingLabel=Mengekstrak file...
ButtonStopExtraction=&Hentikan ekstraksi
StopExtraction=Apakah Anda yakin ingin menghentikan ekstraksi?
ErrorExtractionAborted=Ekstraksi dibatalkan
ErrorExtractionFailed=Ekstraksi gagal: %1

; *** Archive extraction failure details
ArchiveIncorrectPassword=Kata sandi salah
ArchiveIsCorrupted=Arsip rusak
ArchiveUnsupportedFormat=Format arsip tidak didukung

; *** "Preparing to Install" wizard page
WizardPreparing=Mempersiapkan Instalasi
PreparingDesc=Instalasi sedang mempersiapkan instalasi [name] di komputer Anda.
PreviousInstallNotCompleted=Instalasi/penghapusan program sebelumnya belum selesai. Anda perlu memulai ulang komputer untuk menyelesaikan instalasi tersebut.%n%nSetelah memulai ulang komputer, jalankan Instalasi lagi untuk menyelesaikan instalasi [name].
CannotContinue=Instalasi tidak dapat dilanjutkan. Harap klik Batal untuk keluar.
ApplicationsFound=Aplikasi berikut sedang menggunakan file yang perlu diperbarui oleh Instalasi. Disarankan agar Anda mengizinkan Instalasi menutup aplikasi ini secara otomatis.
ApplicationsFound2=Aplikasi berikut sedang menggunakan file yang perlu diperbarui oleh Instalasi. Disarankan agar Anda mengizinkan Instalasi menutup aplikasi ini secara otomatis. Setelah instalasi selesai, Instalasi akan mencoba memulai ulang aplikasi tersebut.
CloseApplications=&Tutup aplikasi secara otomatis
DontCloseApplications=&Jangan tutup aplikasi
ErrorCloseApplications=Instalasi tidak dapat menutup semua aplikasi secara otomatis. Disarankan agar Anda menutup semua aplikasi yang menggunakan file yang perlu diperbarui oleh Instalasi sebelum melanjutkan.
PrepareToInstallNeedsRestart=Instalasi harus memulai ulang komputer Anda. Setelah memulai ulang komputer, jalankan Instalasi lagi untuk menyelesaikan instalasi [name].%n%nApakah Anda ingin memulai ulang sekarang?

; *** "Installing" wizard page
WizardInstalling=Menginstal
InstallingLabel=Harap tunggu sementara Instalasi menginstal [name] di komputer Anda.

; *** "Setup Completed" wizard page
FinishedHeadingLabel=Menyelesaikan Panduan Instalasi [name]
FinishedLabelNoIcons=Instalasi telah selesai menginstal [name] di komputer Anda.
FinishedLabel=Instalasi telah selesai menginstal [name] di komputer Anda. Aplikasi dapat dijalankan dengan memilih pintasan yang telah diinstal.
ClickFinish=Klik Selesai untuk keluar dari Instalasi.
FinishedRestartLabel=Untuk menyelesaikan instalasi [name], Instalasi harus memulai ulang komputer Anda. Apakah Anda ingin memulai ulang sekarang?
FinishedRestartMessage=Untuk menyelesaikan instalasi [name], Instalasi harus memulai ulang komputer Anda.%n%nApakah Anda ingin memulai ulang sekarang?
ShowReadmeCheck=Ya, saya ingin melihat file README
YesRadio=&Ya, mulai ulang komputer sekarang
NoRadio=&Tidak, saya akan memulai ulang komputer nanti
RunEntryExec=Jalankan %1
RunEntryShellExec=Lihat %1

; *** "Setup Needs the Next Disk" stuff
ChangeDiskTitle=Instalasi Memerlukan Disk Berikutnya
SelectDiskLabel2=Harap masukkan Disk %1 dan klik OK.%n%nJika file di disk ini dapat ditemukan di folder selain yang ditampilkan di bawah ini, masukkan jalur yang benar atau klik Telusuri.
PathLabel=&Jalur:
FileNotInDir2=File "%1" tidak dapat ditemukan di "%2". Harap masukkan disk yang benar atau pilih folder lain.
SelectDirectoryLabel=Harap tentukan lokasi disk berikutnya.

; *** Installation phase messages
SetupAborted=Instalasi belum selesai.%n%nHarap perbaiki masalah dan jalankan Instalasi lagi.
AbortRetryIgnoreSelectAction=Pilih tindakan
AbortRetryIgnoreRetry=&Coba lagi
AbortRetryIgnoreIgnore=&Abaikan kesalahan dan lanjutkan
AbortRetryIgnoreCancel=Batalkan instalasi
RetryCancelSelectAction=Pilih tindakan
RetryCancelRetry=&Coba lagi
RetryCancelCancel=Batal

; *** Installation status messages
StatusClosingApplications=Menutup aplikasi...
StatusCreateDirs=Membuat direktori...
StatusExtractFiles=Mengekstrak file...
StatusDownloadFiles=Mengunduh file...
StatusCreateIcons=Membuat pintasan...
StatusCreateIniEntries=Membuat entri INI...
StatusCreateRegistryEntries=Membuat entri registri...
StatusRegisterFiles=Mendaftarkan file...
StatusSavingUninstall=Menyimpan informasi hapus instalasi...
StatusRunProgram=Menyelesaikan instalasi...
StatusRestartingApplications=Memulai ulang aplikasi...
StatusRollback=Membatalkan perubahan...

; *** Misc. errors
ErrorInternal2=Kesalahan internal: %1
ErrorFunctionFailedNoCode=%1 gagal
ErrorFunctionFailed=%1 gagal; kode %2
ErrorFunctionFailedWithMessage=%1 gagal; kode %2.%n%3
ErrorExecutingProgram=Tidak dapat menjalankan file:%n%1

; *** Registry errors
ErrorRegOpenKey=Kesalahan saat membuka kunci registri:%n%1\%2
ErrorRegCreateKey=Kesalahan saat membuat kunci registri:%n%1\%2
ErrorRegWriteKey=Kesalahan saat menulis ke kunci registri:%n%1\%2

; *** INI errors
ErrorIniEntry=Kesalahan saat membuat entri INI di file "%1".

; *** File copying errors
FileAbortRetryIgnoreSkipNotRecommended=&Lewati file ini (tidak disarankan)
FileAbortRetryIgnoreIgnoreNotRecommended=&Abaikan kesalahan dan lanjutkan (tidak disarankan)
SourceIsCorrupted=File sumber rusak
SourceDoesntExist=File sumber "%1" tidak ada
SourceVerificationFailed=Verifikasi file sumber gagal: %1
VerificationSignatureDoesntExist=File tanda tangan "%1" tidak ada
VerificationSignatureInvalid=File tanda tangan "%1" tidak valid
VerificationKeyNotFound=File tanda tangan "%1" menggunakan kunci yang tidak dikenal
VerificationFileNameIncorrect=Nama file salah
VerificationFileTagIncorrect=Tag file salah
VerificationFileSizeIncorrect=Ukuran file salah
VerificationFileHashIncorrect=Hash file salah
ExistingFileReadOnly2=File yang ada tidak dapat diganti karena ditandai hanya-baca.
ExistingFileReadOnlyRetry=&Hapus atribut hanya-baca dan coba lagi
ExistingFileReadOnlyKeepExisting=&Pertahankan file yang ada
ErrorReadingExistingDest=Terjadi kesalahan saat mencoba membaca file yang ada:
FileExistsSelectAction=Pilih tindakan
FileExists2=File sudah ada.
FileExistsOverwriteExisting=&Timpa file yang ada
FileExistsKeepExisting=&Pertahankan file yang ada
FileExistsOverwriteOrKeepAll=&Lakukan ini untuk konflik berikutnya
ExistingFileNewerSelectAction=Pilih tindakan
ExistingFileNewer2=File yang ada lebih baru daripada file yang akan diinstal oleh Instalasi.
ExistingFileNewerOverwriteExisting=&Timpa file yang ada
ExistingFileNewerKeepExisting=&Pertahankan file yang ada (disarankan)
ExistingFileNewerOverwriteOrKeepAll=&Lakukan ini untuk konflik berikutnya
ErrorChangingAttr=Terjadi kesalahan saat mencoba mengubah atribut file yang ada:
ErrorCreatingTemp=Terjadi kesalahan saat mencoba membuat file di direktori tujuan:
ErrorReadingSource=Terjadi kesalahan saat mencoba membaca file sumber:
ErrorCopying=Terjadi kesalahan saat mencoba menyalin file:
ErrorDownloading=Terjadi kesalahan saat mencoba mengunduh file:
ErrorExtracting=Terjadi kesalahan saat mencoba mengekstrak arsip:
ErrorReplacingExistingFile=Terjadi kesalahan saat mencoba mengganti file yang ada:
ErrorRestartReplace=RestartReplace gagal:
ErrorRenamingTemp=Terjadi kesalahan saat mencoba mengganti nama file di direktori tujuan:
ErrorRegisterServer=Tidak dapat mendaftarkan DLL/OCX: %1
ErrorRegSvr32Failed=RegSvr32 gagal dengan kode keluar %1
ErrorRegisterTypeLib=Tidak dapat mendaftarkan pustaka tipe: %1

; *** Uninstall display name markings
UninstallDisplayNameMark=%1 (%2)
UninstallDisplayNameMarks=%1 (%2, %3)
UninstallDisplayNameMark32Bit=32-bit
UninstallDisplayNameMark64Bit=64-bit
UninstallDisplayNameMarkAllUsers=Semua pengguna
UninstallDisplayNameMarkCurrentUser=Pengguna saat ini

; *** Post-installation errors
ErrorOpeningReadme=Terjadi kesalahan saat mencoba membuka file README.
ErrorRestartingComputer=Instalasi tidak dapat memulai ulang komputer. Harap lakukan secara manual.

; *** Uninstaller messages
UninstallNotFound=File "%1" tidak ada. Tidak dapat menghapus instalasi.
UninstallOpenError=File "%1" tidak dapat dibuka. Tidak dapat menghapus instalasi
UninstallUnsupportedVer=File log hapus instalasi "%1" dalam format yang tidak dikenali oleh versi program hapus instalasi ini. Tidak dapat menghapus instalasi
UninstallUnknownEntry=Ditemukan entri yang tidak dikenal (%1) di log hapus instalasi
ConfirmUninstall=Apakah Anda yakin ingin menghapus %1 dan semua komponennya sepenuhnya?
UninstallOnlyOnWin64=Instalasi ini hanya dapat dihapus di Windows 64-bit.
OnlyAdminCanUninstall=Instalasi ini hanya dapat dihapus oleh pengguna dengan hak administrator.
UninstallStatusLabel=Harap tunggu sementara %1 dihapus dari komputer Anda.
UninstalledAll=%1 berhasil dihapus dari komputer Anda.
UninstalledMost=Hapus instalasi %1 selesai.%n%nBeberapa elemen tidak dapat dihapus. Elemen tersebut dapat dihapus secara manual.
UninstalledAndNeedsRestart=Untuk menyelesaikan hapus instalasi %1, komputer Anda harus dimulai ulang.%n%nApakah Anda ingin memulai ulang sekarang?
UninstallDataCorrupted=File "%1" rusak. Tidak dapat menghapus instalasi

; *** Uninstallation phase messages
ConfirmDeleteSharedFileTitle=Hapus File Bersama?
ConfirmDeleteSharedFile2=Sistem menunjukkan bahwa file bersama berikut tidak lagi digunakan oleh program apa pun. Apakah Anda ingin Hapus Instalasi menghapus file bersama ini?%n%nJika ada program yang masih menggunakan file ini dan file ini dihapus, program tersebut mungkin tidak berfungsi dengan benar. Jika Anda tidak yakin, pilih Tidak. Membiarkan file ini di sistem Anda tidak akan menimbulkan masalah.
SharedFileNameLabel=Nama file:
SharedFileLocationLabel=Lokasi:
WizardUninstalling=Status Hapus Instalasi
StatusUninstalling=Menghapus instalasi %1...

; *** Shutdown block reasons
ShutdownBlockReasonInstallingApp=Menginstal %1.
ShutdownBlockReasonUninstallingApp=Menghapus instalasi %1.

[CustomMessages]

NameAndVersion=%1 versi %2
AdditionalIcons=Pintasan tambahan:
CreateDesktopIcon=Buat pintasan di &desktop
CreateQuickLaunchIcon=Buat pintasan &Quick Launch
ProgramOnTheWeb=%1 di Web
UninstallProgram=Hapus instalasi %1
LaunchProgram=Jalankan %1
AssocFileExtension=&Kaitkan %1 dengan ekstensi file %2
AssocingFileExtension=Mengaitkan %1 dengan ekstensi file %2...
AutoStartProgramGroupDescription=Startup:
AutoStartProgram=Jalankan %1 secara otomatis
AddonHostProgramNotFound=%1 tidak dapat ditemukan di folder yang Anda pilih.%n%nApakah Anda tetap ingin melanjutkan?
