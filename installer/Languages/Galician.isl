; *** Inno Setup version 6.5.0+ Galician messages ***
; Translated for VB Recorder. Based on Default.isl of Inno Setup 7.1.

[LangOptions]
LanguageName=Galego
LanguageID=$0456
LanguageCodePage=1252

[Messages]

; *** Application titles
SetupAppTitle=Instalación
SetupWindowTitle=Instalación - %1
UninstallAppTitle=Desinstalación
UninstallAppFullTitle=Desinstalación de %1

; *** Misc. common
InformationTitle=Información
ConfirmTitle=Confirmar
ErrorTitle=Erro

; *** SetupLdr messages
SetupLdrStartupMessage=Isto instalará %1. Desexa continuar?
LdrCannotCreateTemp=Non foi posible crear un ficheiro temporal. Instalación cancelada
LdrCannotExecTemp=Non foi posible executar o ficheiro no directorio temporal. Instalación cancelada
HelpTextNote=

; *** Startup error messages
LastErrorMessage=%1.%n%nErro %2: %3
SetupFileMissing=Falta o ficheiro %1 no directorio de instalación. Corrixa o problema ou obteña unha copia nova do programa.
SetupFileCorrupt=Os ficheiros de instalación están danados. Obteña unha copia nova do programa.
SetupFileCorruptOrWrongVer=Os ficheiros de instalación están danados ou non son compatibles con esta versión da Instalación. Corrixa o problema ou obteña unha copia nova do programa.
InvalidParameter=Pasouse un parámetro non válido na liña de ordes:%n%n%1
SetupAlreadyRunning=A Instalación xa está en execución.
WindowsVersionNotSupported=Este programa non admite a versión de Windows que executa o seu computador.
WindowsServicePackRequired=Este programa require %1 Service Pack %2 ou posterior.
NotOnThisPlatform=Este programa non se executará en %1.
OnlyOnThisPlatform=Este programa debe executarse en %1.
OnlyOnTheseArchitectures=Este programa só se pode instalar en versións de Windows deseñadas para as seguintes arquitecturas de procesador:%n%n%1
WinVersionTooLowError=Este programa require %1 versión %2 ou posterior.
WinVersionTooHighError=Este programa non se pode instalar en %1 versión %2 ou posterior.
AdminPrivilegesRequired=Debe iniciar sesión como administrador para instalar este programa.
PowerUserPrivilegesRequired=Debe iniciar sesión como administrador ou como membro do grupo Usuarios avanzados para instalar este programa.
SetupAppRunningError=A Instalación detectou que %1 está en execución.%n%nPeche agora todas as súas instancias e prema Aceptar para continuar, ou Cancelar para saír.
UninstallAppRunningError=A Desinstalación detectou que %1 está en execución.%n%nPeche agora todas as súas instancias e prema Aceptar para continuar, ou Cancelar para saír.

; *** Startup questions
PrivilegesRequiredOverrideTitle=Seleccione o modo de instalación
PrivilegesRequiredOverrideInstruction=Seleccione o modo de instalación
PrivilegesRequiredOverrideText1=%1 pódese instalar para todos os usuarios (require privilexios de administrador) ou só para vostede.
PrivilegesRequiredOverrideText2=%1 pódese instalar só para vostede ou para todos os usuarios (require privilexios de administrador).
PrivilegesRequiredOverrideAllUsers=Instalar para &todos os usuarios
PrivilegesRequiredOverrideAllUsersRecommended=Instalar para &todos os usuarios (recomendado)
PrivilegesRequiredOverrideCurrentUser=Instalar só para &min
PrivilegesRequiredOverrideCurrentUserRecommended=Instalar só para &min (recomendado)

; *** Misc. errors
ErrorCreatingDir=A Instalación non puido crear o directorio "%1"
ErrorTooManyFilesInDir=Non foi posible crear un ficheiro no directorio "%1" porque contén demasiados ficheiros

; *** Setup common messages
ExitSetupTitle=Saír da Instalación
ExitSetupMessage=A instalación non está completa. Se sae agora, o programa non se instalará.%n%nPode executar a Instalación noutro momento para completar a instalación.%n%nSaír da Instalación?
AboutSetupMenuItem=&Acerca da Instalación...
AboutSetupTitle=Acerca da Instalación
AboutSetupMessage=%1 %2%n%3%n%nPáxina web de %1:%n%4
AboutSetupNote=
TranslatorNote=

; *** Buttons
ButtonBack=< &Atrás
ButtonNext=&Seguinte >
ButtonInstall=&Instalar
ButtonOK=Aceptar
ButtonCancel=Cancelar
ButtonYes=&Si
ButtonYesToAll=Si a &todo
ButtonNo=&Non
ButtonNoToAll=N&on a todo
ButtonFinish=&Finalizar
ButtonBrowse=&Examinar...
ButtonWizardBrowse=E&xaminar...
ButtonNewFolder=&Crear un cartafol novo

; *** "Select Language" dialog messages
SelectLanguageTitle=Seleccione o idioma da instalación
SelectLanguageLabel=Seleccione o idioma que se usará durante a instalación.

; *** Common wizard text
ClickNext=Prema Seguinte para continuar, ou Cancelar para saír da Instalación.
BeveledLabel=
BrowseDialogTitle=Buscar cartafol
BrowseDialogLabel=Seleccione un cartafol da lista seguinte e prema Aceptar.
NewFolderName=Cartafol novo

; *** "Welcome" wizard page
WelcomeLabel1=Benvido ao asistente de instalación de [name]
WelcomeLabel2=Isto instalará [name/ver] no seu computador.%n%nRecoméndase pechar todas as outras aplicacións antes de continuar.

; *** "Password" wizard page
WizardPassword=Contrasinal
PasswordLabel1=Esta instalación está protexida por contrasinal.
PasswordLabel3=Introduza o contrasinal e prema Seguinte para continuar. Os contrasinais distinguen maiúsculas e minúsculas.
PasswordEditLabel=&Contrasinal:
IncorrectPassword=O contrasinal introducido non é correcto. Ténteo de novo.

; *** "License Agreement" wizard page
WizardLicense=Acordo de licenza
LicenseLabel=Lea a seguinte información importante antes de continuar.
LicenseLabel3=Lea o seguinte acordo de licenza. Debe aceptar os termos deste acordo antes de continuar coa instalación.
LicenseAccepted=&Acepto o acordo
LicenseNotAccepted=&Non acepto o acordo

; *** "Information" wizard pages
WizardInfoBefore=Información
InfoBeforeLabel=Lea a seguinte información importante antes de continuar.
InfoBeforeClickLabel=Cando estea preparado para continuar coa Instalación, prema Seguinte.
WizardInfoAfter=Información
InfoAfterLabel=Lea a seguinte información importante antes de continuar.
InfoAfterClickLabel=Cando estea preparado para continuar coa Instalación, prema Seguinte.

; *** "User Information" wizard page
WizardUserInfo=Información do usuario
UserInfoDesc=Introduza a súa información.
UserInfoName=Nome de &usuario:
UserInfoOrg=&Organización:
UserInfoSerial=Número de &serie:
UserInfoNameRequired=Debe introducir un nome.

; *** "Select Destination Location" wizard page
WizardSelectDir=Seleccione a localización de destino
SelectDirDesc=Onde se debe instalar [name]?
SelectDirLabel3=A Instalación instalará [name] no seguinte cartafol.
SelectDirBrowseLabel=Para continuar, prema Seguinte. Se desexa seleccionar outro cartafol, prema Examinar.
DiskSpaceGBLabel=Requírense polo menos [gb] GB de espazo libre no disco.
DiskSpaceMBLabel=Requírense polo menos [mb] MB de espazo libre no disco.
CannotInstallToNetworkDrive=A Instalación non pode instalar nunha unidade de rede.
CannotInstallToUNCPath=A Instalación non pode instalar nunha ruta UNC.
InvalidPath=Debe introducir unha ruta completa coa letra da unidade; por exemplo:%n%nC:\App%n%nou unha ruta UNC coa forma:%n%n\\servidor\recurso
InvalidDrive=A unidade ou o recurso compartido UNC seleccionado non existe ou non é accesible. Seleccione outro.
DiskSpaceWarningTitle=Non hai espazo suficiente no disco
DiskSpaceWarning=A Instalación require polo menos %1 KB de espazo libre para instalar, pero a unidade seleccionada só ten %2 KB dispoñibles.%n%nDesexa continuar de todos os xeitos?
DirNameTooLong=O nome do cartafol ou a ruta é demasiado longo.
InvalidDirName=O nome do cartafol non é válido.
BadDirName32=Os nomes de cartafol non poden incluír ningún dos seguintes caracteres:%n%n%1
DirExistsTitle=O cartafol existe
DirExists=O cartafol:%n%n%1%n%nxa existe. Desexa instalar nese cartafol de todos os xeitos?
DirDoesntExistTitle=O cartafol non existe
DirDoesntExist=O cartafol:%n%n%1%n%nnon existe. Desexa crear o cartafol?

; *** "Select Components" wizard page
WizardSelectComponents=Seleccione os compoñentes
SelectComponentsDesc=Que compoñentes se deben instalar?
SelectComponentsLabel2=Seleccione os compoñentes que desexa instalar; desmarque os compoñentes que non desexa instalar. Prema Seguinte cando estea preparado para continuar.
FullInstallation=Instalación completa
CompactInstallation=Instalación compacta
CustomInstallation=Instalación personalizada
NoUninstallWarningTitle=Os compoñentes existen
NoUninstallWarning=A Instalación detectou que os seguintes compoñentes xa están instalados no seu computador:%n%n%1%n%nDesmarcar estes compoñentes non os desinstalará.%n%nDesexa continuar de todos os xeitos?
ComponentSize1=%1 KB
ComponentSize2=%1 MB
ComponentsDiskSpaceGBLabel=A selección actual require polo menos [gb] GB de espazo no disco.
ComponentsDiskSpaceMBLabel=A selección actual require polo menos [mb] MB de espazo no disco.

; *** "Select Additional Tasks" wizard page
WizardSelectTasks=Seleccione as tarefas adicionais
SelectTasksDesc=Que tarefas adicionais se deben realizar?
SelectTasksLabel2=Seleccione as tarefas adicionais que desexa que realice a Instalación mentres instala [name] e prema Seguinte.

; *** "Select Start Menu Folder" wizard page
WizardSelectProgramGroup=Seleccione o cartafol do menú Inicio
SelectStartMenuFolderDesc=Onde debe colocar a Instalación os atallos do programa?
SelectStartMenuFolderLabel3=A Instalación creará os atallos do programa no seguinte cartafol do menú Inicio.
SelectStartMenuFolderBrowseLabel=Para continuar, prema Seguinte. Se desexa seleccionar outro cartafol, prema Examinar.
MustEnterGroupName=Debe introducir un nome de cartafol.
GroupNameTooLong=O nome do cartafol ou a ruta é demasiado longo.
InvalidGroupName=O nome do cartafol non é válido.
BadGroupName=O nome do cartafol non pode incluír ningún dos seguintes caracteres:%n%n%1
NoProgramGroupCheck2=&Non crear un cartafol no menú Inicio

; *** "Ready to Install" wizard page
WizardReady=Preparado para instalar
ReadyLabel1=A Instalación está preparada para comezar a instalar [name] no seu computador.
ReadyLabel2a=Prema Instalar para continuar coa instalación, ou prema Atrás se desexa revisar ou cambiar algunha configuración.
ReadyLabel2b=Prema Instalar para continuar coa instalación.
ReadyMemoUserInfo=Información do usuario:
ReadyMemoDir=Localización de destino:
ReadyMemoType=Tipo de instalación:
ReadyMemoComponents=Compoñentes seleccionados:
ReadyMemoGroup=Cartafol do menú Inicio:
ReadyMemoTasks=Tarefas adicionais:

; *** TDownloadWizardPage wizard page and DownloadTemporaryFile
DownloadingLabel2=Descargando ficheiros...
ButtonStopDownload=&Deter a descarga
StopDownload=Está seguro de que desexa deter a descarga?
ErrorDownloadAborted=Descarga cancelada
ErrorDownloadFailed=Fallou a descarga: %1 %2
ErrorDownloadSizeFailed=Fallou a obtención do tamaño: %1 %2
ErrorProgress=Progreso non válido: %1 de %2
ErrorFileSize=Tamaño de ficheiro non válido: esperábase %1, atopouse %2

; *** TExtractionWizardPage wizard page and ExtractArchive
ExtractingLabel=Extraendo ficheiros...
ButtonStopExtraction=&Deter a extracción
StopExtraction=Está seguro de que desexa deter a extracción?
ErrorExtractionAborted=Extracción cancelada
ErrorExtractionFailed=Fallou a extracción: %1

; *** Archive extraction failure details
ArchiveIncorrectPassword=O contrasinal é incorrecto
ArchiveIsCorrupted=O arquivo está danado
ArchiveUnsupportedFormat=O formato do arquivo non é compatible

; *** "Preparing to Install" wizard page
WizardPreparing=Preparando a instalación
PreparingDesc=A Instalación está preparando a instalación de [name] no seu computador.
PreviousInstallNotCompleted=A instalación ou eliminación dun programa anterior non se completou. Terá que reiniciar o computador para completar esa instalación.%n%nDespois de reiniciar o computador, execute de novo a Instalación para completar a instalación de [name].
CannotContinue=A Instalación non pode continuar. Prema Cancelar para saír.
ApplicationsFound=As seguintes aplicacións están a usar ficheiros que a Instalación debe actualizar. Recoméndase que permita que a Instalación peche estas aplicacións automaticamente.
ApplicationsFound2=As seguintes aplicacións están a usar ficheiros que a Instalación debe actualizar. Recoméndase que permita que a Instalación peche estas aplicacións automaticamente. Despois de completar a instalación, a Instalación intentará reiniciar as aplicacións.
CloseApplications=&Pechar as aplicacións automaticamente
DontCloseApplications=&Non pechar as aplicacións
ErrorCloseApplications=A Instalación non puido pechar automaticamente todas as aplicacións. Recoméndase pechar todas as aplicacións que usan ficheiros que a Instalación debe actualizar antes de continuar.
PrepareToInstallNeedsRestart=A Instalación debe reiniciar o seu computador. Despois de reiniciar o computador, execute de novo a Instalación para completar a instalación de [name].%n%nDesexa reiniciar agora?

; *** "Installing" wizard page
WizardInstalling=Instalando
InstallingLabel=Agarde mentres a Instalación instala [name] no seu computador.

; *** "Setup Completed" wizard page
FinishedHeadingLabel=Completando o asistente de instalación de [name]
FinishedLabelNoIcons=A Instalación rematou de instalar [name] no seu computador.
FinishedLabel=A Instalación rematou de instalar [name] no seu computador. Pode iniciar a aplicación seleccionando os atallos instalados.
ClickFinish=Prema Finalizar para saír da Instalación.
FinishedRestartLabel=Para completar a instalación de [name], a Instalación debe reiniciar o seu computador. Desexa reiniciar agora?
FinishedRestartMessage=Para completar a instalación de [name], a Instalación debe reiniciar o seu computador.%n%nDesexa reiniciar agora?
ShowReadmeCheck=Si, desexo ver o ficheiro README
YesRadio=&Si, reiniciar o computador agora
NoRadio=&Non, reiniciarei o computador máis tarde
RunEntryExec=Executar %1
RunEntryShellExec=Ver %1

; *** "Setup Needs the Next Disk" stuff
ChangeDiskTitle=A Instalación necesita o seguinte disco
SelectDiskLabel2=Insira o disco %1 e prema Aceptar.%n%nSe os ficheiros deste disco se poden atopar nun cartafol distinto do que se mostra a continuación, introduza a ruta correcta ou prema Examinar.
PathLabel=&Ruta:
FileNotInDir2=Non se puido atopar o ficheiro "%1" en "%2". Insira o disco correcto ou seleccione outro cartafol.
SelectDirectoryLabel=Indique a localización do seguinte disco.

; *** Installation phase messages
SetupAborted=A instalación non se completou.%n%nCorrixa o problema e execute de novo a Instalación.
AbortRetryIgnoreSelectAction=Seleccione unha acción
AbortRetryIgnoreRetry=&Tentar de novo
AbortRetryIgnoreIgnore=&Ignorar o erro e continuar
AbortRetryIgnoreCancel=Cancelar a instalación
RetryCancelSelectAction=Seleccione unha acción
RetryCancelRetry=&Tentar de novo
RetryCancelCancel=Cancelar

; *** Installation status messages
StatusClosingApplications=Pechando as aplicacións...
StatusCreateDirs=Creando os directorios...
StatusExtractFiles=Extraendo os ficheiros...
StatusDownloadFiles=Descargando os ficheiros...
StatusCreateIcons=Creando os atallos...
StatusCreateIniEntries=Creando as entradas INI...
StatusCreateRegistryEntries=Creando as entradas do rexistro...
StatusRegisterFiles=Rexistrando os ficheiros...
StatusSavingUninstall=Gardando a información de desinstalación...
StatusRunProgram=Finalizando a instalación...
StatusRestartingApplications=Reiniciando as aplicacións...
StatusRollback=Desfacendo os cambios...

; *** Misc. errors
ErrorInternal2=Erro interno: %1
ErrorFunctionFailedNoCode=%1 fallou
ErrorFunctionFailed=%1 fallou; código %2
ErrorFunctionFailedWithMessage=%1 fallou; código %2.%n%3
ErrorExecutingProgram=Non foi posible executar o ficheiro:%n%1

; *** Registry errors
ErrorRegOpenKey=Erro ao abrir a clave do rexistro:%n%1\%2
ErrorRegCreateKey=Erro ao crear a clave do rexistro:%n%1\%2
ErrorRegWriteKey=Erro ao escribir na clave do rexistro:%n%1\%2

; *** INI errors
ErrorIniEntry=Erro ao crear a entrada INI no ficheiro "%1".

; *** File copying errors
FileAbortRetryIgnoreSkipNotRecommended=&Omitir este ficheiro (non recomendado)
FileAbortRetryIgnoreIgnoreNotRecommended=&Ignorar o erro e continuar (non recomendado)
SourceIsCorrupted=O ficheiro de orixe está danado
SourceDoesntExist=O ficheiro de orixe "%1" non existe
SourceVerificationFailed=Fallou a verificación do ficheiro de orixe: %1
VerificationSignatureDoesntExist=O ficheiro de sinatura "%1" non existe
VerificationSignatureInvalid=O ficheiro de sinatura "%1" non é válido
VerificationKeyNotFound=O ficheiro de sinatura "%1" usa unha clave descoñecida
VerificationFileNameIncorrect=O nome do ficheiro é incorrecto
VerificationFileTagIncorrect=A etiqueta do ficheiro é incorrecta
VerificationFileSizeIncorrect=O tamaño do ficheiro é incorrecto
VerificationFileHashIncorrect=O hash do ficheiro é incorrecto
ExistingFileReadOnly2=Non se puido substituír o ficheiro existente porque está marcado como de só lectura.
ExistingFileReadOnlyRetry=&Quitar o atributo de só lectura e tentar de novo
ExistingFileReadOnlyKeepExisting=&Manter o ficheiro existente
ErrorReadingExistingDest=Produciuse un erro ao tentar ler o ficheiro existente:
FileExistsSelectAction=Seleccione unha acción
FileExists2=O ficheiro xa existe.
FileExistsOverwriteExisting=&Sobrescribir o ficheiro existente
FileExistsKeepExisting=&Manter o ficheiro existente
FileExistsOverwriteOrKeepAll=&Facer isto para os seguintes conflitos
ExistingFileNewerSelectAction=Seleccione unha acción
ExistingFileNewer2=O ficheiro existente é máis recente que o que a Instalación tenta instalar.
ExistingFileNewerOverwriteExisting=&Sobrescribir o ficheiro existente
ExistingFileNewerKeepExisting=&Manter o ficheiro existente (recomendado)
ExistingFileNewerOverwriteOrKeepAll=&Facer isto para os seguintes conflitos
ErrorChangingAttr=Produciuse un erro ao tentar cambiar os atributos do ficheiro existente:
ErrorCreatingTemp=Produciuse un erro ao tentar crear un ficheiro no directorio de destino:
ErrorReadingSource=Produciuse un erro ao tentar ler o ficheiro de orixe:
ErrorCopying=Produciuse un erro ao tentar copiar un ficheiro:
ErrorDownloading=Produciuse un erro ao tentar descargar un ficheiro:
ErrorExtracting=Produciuse un erro ao tentar extraer un arquivo:
ErrorReplacingExistingFile=Produciuse un erro ao tentar substituír o ficheiro existente:
ErrorRestartReplace=Fallou RestartReplace:
ErrorRenamingTemp=Produciuse un erro ao tentar renomear un ficheiro no directorio de destino:
ErrorRegisterServer=Non foi posible rexistrar a DLL/OCX: %1
ErrorRegSvr32Failed=RegSvr32 fallou co código de saída %1
ErrorRegisterTypeLib=Non foi posible rexistrar a biblioteca de tipos: %1

; *** Uninstall display name markings
UninstallDisplayNameMark=%1 (%2)
UninstallDisplayNameMarks=%1 (%2, %3)
UninstallDisplayNameMark32Bit=32 bits
UninstallDisplayNameMark64Bit=64 bits
UninstallDisplayNameMarkAllUsers=Todos os usuarios
UninstallDisplayNameMarkCurrentUser=Usuario actual

; *** Post-installation errors
ErrorOpeningReadme=Produciuse un erro ao tentar abrir o ficheiro README.
ErrorRestartingComputer=A Instalación non puido reiniciar o computador. Fágao manualmente.

; *** Uninstaller messages
UninstallNotFound=O ficheiro "%1" non existe. Non se pode desinstalar.
UninstallOpenError=Non se puido abrir o ficheiro "%1". Non se pode desinstalar
UninstallUnsupportedVer=O ficheiro de rexistro de desinstalación "%1" ten un formato que esta versión do desinstalador non recoñece. Non se pode desinstalar
UninstallUnknownEntry=Atopouse unha entrada descoñecida (%1) no rexistro de desinstalación
ConfirmUninstall=Está seguro de que desexa eliminar completamente %1 e todos os seus compoñentes?
UninstallOnlyOnWin64=Esta instalación só se pode desinstalar en Windows de 64 bits.
OnlyAdminCanUninstall=Esta instalación só a pode desinstalar un usuario con privilexios de administrador.
UninstallStatusLabel=Agarde mentres se elimina %1 do seu computador.
UninstalledAll=%1 eliminouse correctamente do seu computador.
UninstalledMost=Completouse a desinstalación de %1.%n%nAlgúns elementos non se puideron eliminar. Pódense eliminar manualmente.
UninstalledAndNeedsRestart=Para completar a desinstalación de %1, debe reiniciar o seu computador.%n%nDesexa reiniciar agora?
UninstallDataCorrupted=O ficheiro "%1" está danado. Non se pode desinstalar

; *** Uninstallation phase messages
ConfirmDeleteSharedFileTitle=Eliminar o ficheiro compartido?
ConfirmDeleteSharedFile2=O sistema indica que o seguinte ficheiro compartido xa non o usa ningún programa. Desexa que a Desinstalación elimine este ficheiro compartido?%n%nSe algún programa aínda usa este ficheiro e se elimina, ese programa pode non funcionar correctamente. Se non está seguro, escolla Non. Deixar o ficheiro no sistema non causará ningún dano.
SharedFileNameLabel=Nome do ficheiro:
SharedFileLocationLabel=Localización:
WizardUninstalling=Estado da desinstalación
StatusUninstalling=Desinstalando %1...

; *** Shutdown block reasons
ShutdownBlockReasonInstallingApp=Instalando %1.
ShutdownBlockReasonUninstallingApp=Desinstalando %1.

[CustomMessages]

NameAndVersion=%1 versión %2
AdditionalIcons=Atallos adicionais:
CreateDesktopIcon=Crear un atallo no &escritorio
CreateQuickLaunchIcon=Crear un atallo de &Inicio rápido
ProgramOnTheWeb=%1 na Web
UninstallProgram=Desinstalar %1
LaunchProgram=Iniciar %1
AssocFileExtension=&Asociar %1 coa extensión de ficheiro %2
AssocingFileExtension=Asociando %1 coa extensión de ficheiro %2...
AutoStartProgramGroupDescription=Inicio:
AutoStartProgram=Iniciar %1 automaticamente
AddonHostProgramNotFound=Non se puido atopar %1 no cartafol seleccionado.%n%nDesexa continuar de todos os xeitos?
