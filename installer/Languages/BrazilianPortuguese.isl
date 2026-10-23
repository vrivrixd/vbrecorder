; *** Inno Setup version 6.5.0+ Brazilian Portuguese messages ***
; Revised for VB Recorder, in the terms Windows itself uses in Brazilian
; Portuguese. Based on Default.isl of Inno Setup 7.1.

[LangOptions]
LanguageName=Português (Brasil)
LanguageID=$0416
LanguageCodePage=1252

[Messages]

; *** Application titles
SetupAppTitle=Instalação
SetupWindowTitle=Instalação - %1
UninstallAppTitle=Desinstalação
UninstallAppFullTitle=Desinstalação do %1

; *** Misc. common
InformationTitle=Informação
ConfirmTitle=Confirmação
ErrorTitle=Erro

; *** SetupLdr messages
SetupLdrStartupMessage=O %1 será instalado. Deseja continuar?
LdrCannotCreateTemp=Não foi possível criar um arquivo temporário. Instalação cancelada
LdrCannotExecTemp=Não foi possível executar um arquivo na pasta temporária. Instalação cancelada
HelpTextNote=

; *** Startup error messages
LastErrorMessage=%1.%n%nErro %2: %3
SetupFileMissing=O arquivo %1 não foi encontrado na pasta de instalação. Corrija o problema ou obtenha uma nova cópia do programa.
SetupFileCorrupt=Os arquivos de instalação estão corrompidos. Obtenha uma nova cópia do programa.
SetupFileCorruptOrWrongVer=Os arquivos de instalação estão corrompidos ou não são compatíveis com esta versão do instalador. Corrija o problema ou obtenha uma nova cópia do programa.
InvalidParameter=Um parâmetro inválido foi informado na linha de comando:%n%n%1
SetupAlreadyRunning=A instalação já está em andamento.
WindowsVersionNotSupported=Este programa não é compatível com a versão do Windows instalada no seu computador.
WindowsServicePackRequired=Este programa requer o %1 Service Pack %2 ou posterior.
NotOnThisPlatform=Este programa não funciona no %1.
OnlyOnThisPlatform=Este programa só funciona no %1.
OnlyOnTheseArchitectures=Este programa só pode ser instalado em versões do Windows feitas para as seguintes arquiteturas de processador:%n%n%1
WinVersionTooLowError=Este programa requer o %1 versão %2 ou posterior.
WinVersionTooHighError=Este programa não pode ser instalado no %1 versão %2 ou posterior.
AdminPrivilegesRequired=É necessário entrar como administrador para instalar este programa.
PowerUserPrivilegesRequired=É necessário entrar como administrador ou como membro do grupo Usuários Avançados para instalar este programa.
SetupAppRunningError=A instalação detectou que o %1 está em execução.%n%nFeche todas as instâncias dele agora e clique em OK para continuar, ou em Cancelar para sair.
UninstallAppRunningError=A desinstalação detectou que o %1 está em execução.%n%nFeche todas as instâncias dele agora e clique em OK para continuar, ou em Cancelar para sair.

; *** Startup questions
PrivilegesRequiredOverrideTitle=Selecione o Modo de Instalação
PrivilegesRequiredOverrideInstruction=Selecione o modo de instalação
PrivilegesRequiredOverrideText1=O %1 pode ser instalado para todos os usuários (requer privilégios de administrador) ou somente para você.
PrivilegesRequiredOverrideText2=O %1 pode ser instalado somente para você ou para todos os usuários (requer privilégios de administrador).
PrivilegesRequiredOverrideAllUsers=Instalar para &todos os usuários
PrivilegesRequiredOverrideAllUsersRecommended=Instalar para &todos os usuários (recomendado)
PrivilegesRequiredOverrideCurrentUser=Instalar somente para &mim
PrivilegesRequiredOverrideCurrentUserRecommended=Instalar somente para &mim (recomendado)

; *** Misc. errors
ErrorCreatingDir=Não foi possível criar a pasta "%1"
ErrorTooManyFilesInDir=Não foi possível criar um arquivo na pasta "%1" porque ela contém arquivos demais

; *** Setup common messages
ExitSetupTitle=Sair da Instalação
ExitSetupMessage=A instalação não foi concluída. Se você sair agora, o programa não será instalado.%n%nVocê pode executar a instalação novamente mais tarde para concluí-la.%n%nDeseja sair da instalação?
AboutSetupMenuItem=&Sobre o Instalador...
AboutSetupTitle=Sobre o Instalador
AboutSetupMessage=%1 versão %2%n%3%n%nSite do %1:%n%4
AboutSetupNote=
TranslatorNote=

; *** Buttons
ButtonBack=< &Voltar
ButtonNext=&Avançar >
ButtonInstall=&Instalar
ButtonOK=OK
ButtonCancel=Cancelar
ButtonYes=&Sim
ButtonYesToAll=Sim para &Todos
ButtonNo=&Não
ButtonNoToAll=Nã&o para Todos
ButtonFinish=&Concluir
ButtonBrowse=&Procurar...
ButtonWizardBrowse=P&rocurar...
ButtonNewFolder=&Criar Nova Pasta

; *** "Select Language" dialog messages
SelectLanguageTitle=Idioma da Instalação
SelectLanguageLabel=Selecione o idioma a ser usado durante a instalação.

; *** Common wizard text
ClickNext=Clique em Avançar para continuar ou em Cancelar para sair da instalação.
BeveledLabel=
BrowseDialogTitle=Procurar Pasta
BrowseDialogLabel=Selecione uma pasta na lista abaixo e clique em OK.
NewFolderName=Nova Pasta

; *** "Welcome" wizard page
WelcomeLabel1=Bem-vindo ao Assistente de Instalação do [name]
WelcomeLabel2=Este assistente instalará o [name/ver] no seu computador.%n%nRecomendamos fechar todos os outros aplicativos antes de continuar.

; *** "Password" wizard page
WizardPassword=Senha
PasswordLabel1=Esta instalação é protegida por senha.
PasswordLabel3=Digite a senha e clique em Avançar para continuar. A senha diferencia maiúsculas de minúsculas.
PasswordEditLabel=&Senha:
IncorrectPassword=A senha digitada está incorreta. Tente novamente.

; *** "License Agreement" wizard page
WizardLicense=Contrato de Licença
LicenseLabel=Leia as informações importantes a seguir antes de continuar.
LicenseLabel3=Leia o Contrato de Licença a seguir. Você precisa aceitar os termos deste contrato antes de continuar a instalação.
LicenseAccepted=Eu &aceito o contrato
LicenseNotAccepted=Eu &não aceito o contrato

; *** "Information" wizard pages
WizardInfoBefore=Informações
InfoBeforeLabel=Leia as informações importantes a seguir antes de continuar.
InfoBeforeClickLabel=Quando estiver pronto para continuar a instalação, clique em Avançar.
WizardInfoAfter=Informações
InfoAfterLabel=Leia as informações importantes a seguir antes de continuar.
InfoAfterClickLabel=Quando estiver pronto para continuar a instalação, clique em Avançar.

; *** "User Information" wizard page
WizardUserInfo=Informações do Usuário
UserInfoDesc=Digite suas informações.
UserInfoName=&Nome de usuário:
UserInfoOrg=&Organização:
UserInfoSerial=Número de &série:
UserInfoNameRequired=É necessário digitar um nome.

; *** "Select Destination Location" wizard page
WizardSelectDir=Local de Destino
SelectDirDesc=Onde o [name] deve ser instalado?
SelectDirLabel3=O [name] será instalado na pasta a seguir.
SelectDirBrowseLabel=Para continuar, clique em Avançar. Para escolher outra pasta, clique em Procurar.
DiskSpaceGBLabel=São necessários pelo menos [gb] GB de espaço livre em disco.
DiskSpaceMBLabel=São necessários pelo menos [mb] MB de espaço livre em disco.
CannotInstallToNetworkDrive=Não é possível instalar em uma unidade de rede.
CannotInstallToUNCPath=Não é possível instalar em um caminho UNC.
InvalidPath=Digite um caminho completo, com a letra da unidade; por exemplo:%n%nC:\Aplicativo%n%nou um caminho UNC no formato:%n%n\\servidor\compartilhamento
InvalidDrive=A unidade ou o compartilhamento UNC selecionado não existe ou não está acessível. Selecione outro.
DiskSpaceWarningTitle=Espaço em Disco Insuficiente
DiskSpaceWarning=A instalação requer pelo menos %1 KB de espaço livre, mas a unidade selecionada tem apenas %2 KB disponíveis.%n%nDeseja continuar mesmo assim?
DirNameTooLong=O nome ou o caminho da pasta é muito longo.
InvalidDirName=O nome da pasta não é válido.
BadDirName32=O nome da pasta não pode conter nenhum dos seguintes caracteres:%n%n%1
DirExistsTitle=A Pasta Já Existe
DirExists=A pasta:%n%n%1%n%njá existe. Deseja instalar nela mesmo assim?
DirDoesntExistTitle=A Pasta Não Existe
DirDoesntExist=A pasta:%n%n%1%n%nnão existe. Deseja criá-la?

; *** "Select Components" wizard page
WizardSelectComponents=Componentes
SelectComponentsDesc=Quais componentes devem ser instalados?
SelectComponentsLabel2=Marque os componentes que deseja instalar e desmarque os que não deseja. Clique em Avançar quando estiver pronto para continuar.
FullInstallation=Instalação completa
CompactInstallation=Instalação compacta
CustomInstallation=Instalação personalizada
NoUninstallWarningTitle=Componentes Já Instalados
NoUninstallWarning=A instalação detectou que os seguintes componentes já estão instalados no seu computador:%n%n%1%n%nDesmarcar esses componentes não os desinstalará.%n%nDeseja continuar mesmo assim?
ComponentSize1=%1 KB
ComponentSize2=%1 MB
ComponentsDiskSpaceGBLabel=A seleção atual requer pelo menos [gb] GB de espaço em disco.
ComponentsDiskSpaceMBLabel=A seleção atual requer pelo menos [mb] MB de espaço em disco.

; *** "Select Additional Tasks" wizard page
WizardSelectTasks=Tarefas Adicionais
SelectTasksDesc=Quais tarefas adicionais devem ser executadas?
SelectTasksLabel2=Selecione as tarefas adicionais que a instalação deve executar ao instalar o [name] e clique em Avançar.

; *** "Select Start Menu Folder" wizard page
WizardSelectProgramGroup=Pasta do Menu Iniciar
SelectStartMenuFolderDesc=Onde os atalhos do programa devem ser criados?
SelectStartMenuFolderLabel3=Os atalhos do programa serão criados na seguinte pasta do Menu Iniciar.
SelectStartMenuFolderBrowseLabel=Para continuar, clique em Avançar. Para escolher outra pasta, clique em Procurar.
MustEnterGroupName=É necessário digitar o nome de uma pasta.
GroupNameTooLong=O nome ou o caminho da pasta é muito longo.
InvalidGroupName=O nome da pasta não é válido.
BadGroupName=O nome da pasta não pode conter nenhum dos seguintes caracteres:%n%n%1
NoProgramGroupCheck2=&Não criar uma pasta no Menu Iniciar

; *** "Ready to Install" wizard page
WizardReady=Pronto para Instalar
ReadyLabel1=Tudo pronto para instalar o [name] no seu computador.
ReadyLabel2a=Clique em Instalar para continuar ou em Voltar para revisar ou alterar alguma configuração.
ReadyLabel2b=Clique em Instalar para continuar.
ReadyMemoUserInfo=Informações do usuário:
ReadyMemoDir=Local de destino:
ReadyMemoType=Tipo de instalação:
ReadyMemoComponents=Componentes selecionados:
ReadyMemoGroup=Pasta do Menu Iniciar:
ReadyMemoTasks=Tarefas adicionais:

; *** TDownloadWizardPage wizard page and DownloadTemporaryFile
DownloadingLabel2=Baixando arquivos...
ButtonStopDownload=&Interromper download
StopDownload=Tem certeza de que deseja interromper o download?
ErrorDownloadAborted=Download cancelado
ErrorDownloadFailed=Falha no download: %1 %2
ErrorDownloadSizeFailed=Falha ao obter o tamanho: %1 %2
ErrorProgress=Progresso inválido: %1 de %2
ErrorFileSize=Tamanho de arquivo inválido: esperado %1, encontrado %2

; *** TExtractionWizardPage wizard page and ExtractArchive
ExtractingLabel=Extraindo arquivos...
ButtonStopExtraction=&Interromper extração
StopExtraction=Tem certeza de que deseja interromper a extração?
ErrorExtractionAborted=Extração cancelada
ErrorExtractionFailed=Falha na extração: %1

; *** Archive extraction failure details
ArchiveIncorrectPassword=A senha está incorreta
ArchiveIsCorrupted=O arquivo compactado está corrompido
ArchiveUnsupportedFormat=O formato do arquivo compactado não é compatível

; *** "Preparing to Install" wizard page
WizardPreparing=Preparando a Instalação
PreparingDesc=Preparando a instalação do [name] no seu computador.
PreviousInstallNotCompleted=A instalação ou remoção de um programa anterior não foi concluída. É necessário reiniciar o computador para concluí-la.%n%nDepois de reiniciar o computador, execute a instalação novamente para concluir a instalação do [name].
CannotContinue=Não é possível continuar a instalação. Clique em Cancelar para sair.
ApplicationsFound=Os aplicativos a seguir estão usando arquivos que precisam ser atualizados pela instalação. Recomendamos permitir que a instalação feche esses aplicativos automaticamente.
ApplicationsFound2=Os aplicativos a seguir estão usando arquivos que precisam ser atualizados pela instalação. Recomendamos permitir que a instalação feche esses aplicativos automaticamente. Ao final da instalação, a instalação tentará reabrir os aplicativos.
CloseApplications=&Fechar os aplicativos automaticamente
DontCloseApplications=&Não fechar os aplicativos
ErrorCloseApplications=Não foi possível fechar todos os aplicativos automaticamente. Antes de continuar, recomendamos fechar todos os aplicativos que estão usando arquivos que precisam ser atualizados pela instalação.
PrepareToInstallNeedsRestart=É necessário reiniciar o computador. Depois de reiniciá-lo, execute a instalação novamente para concluir a instalação do [name].%n%nDeseja reiniciar agora?

; *** "Installing" wizard page
WizardInstalling=Instalando
InstallingLabel=Aguarde enquanto o [name] é instalado no seu computador.

; *** "Setup Completed" wizard page
FinishedHeadingLabel=Concluindo o Assistente de Instalação do [name]
FinishedLabelNoIcons=A instalação do [name] no seu computador foi concluída.
FinishedLabel=A instalação do [name] no seu computador foi concluída. O aplicativo pode ser aberto pelos atalhos instalados.
ClickFinish=Clique em Concluir para sair da instalação.
FinishedRestartLabel=Para concluir a instalação do [name], é necessário reiniciar o computador. Deseja reiniciar agora?
FinishedRestartMessage=Para concluir a instalação do [name], é necessário reiniciar o computador.%n%nDeseja reiniciar agora?
ShowReadmeCheck=Sim, quero ver o arquivo LEIA-ME
YesRadio=&Sim, reiniciar o computador agora
NoRadio=&Não, vou reiniciar o computador mais tarde
RunEntryExec=Executar o %1
RunEntryShellExec=Abrir o %1

; *** "Setup Needs the Next Disk" stuff
ChangeDiskTitle=Próximo Disco Necessário
SelectDiskLabel2=Insira o disco %1 e clique em OK.%n%nSe os arquivos deste disco estiverem em uma pasta diferente da mostrada abaixo, digite o caminho correto ou clique em Procurar.
PathLabel=&Caminho:
FileNotInDir2=O arquivo "%1" não foi encontrado em "%2". Insira o disco correto ou selecione outra pasta.
SelectDirectoryLabel=Informe o local do próximo disco.

; *** Installation phase messages
SetupAborted=A instalação não foi concluída.%n%nCorrija o problema e execute a instalação novamente.
AbortRetryIgnoreSelectAction=Selecione uma ação
AbortRetryIgnoreRetry=&Tentar novamente
AbortRetryIgnoreIgnore=&Ignorar o erro e continuar
AbortRetryIgnoreCancel=Cancelar a instalação
RetryCancelSelectAction=Selecione uma ação
RetryCancelRetry=&Tentar novamente
RetryCancelCancel=Cancelar

; *** Installation status messages
StatusClosingApplications=Fechando aplicativos...
StatusCreateDirs=Criando pastas...
StatusExtractFiles=Extraindo arquivos...
StatusDownloadFiles=Baixando arquivos...
StatusCreateIcons=Criando atalhos...
StatusCreateIniEntries=Criando entradas INI...
StatusCreateRegistryEntries=Criando entradas no Registro...
StatusRegisterFiles=Registrando arquivos...
StatusSavingUninstall=Salvando as informações de desinstalação...
StatusRunProgram=Concluindo a instalação...
StatusRestartingApplications=Reabrindo os aplicativos...
StatusRollback=Desfazendo as alterações...

; *** Misc. errors
ErrorInternal2=Erro interno: %1
ErrorFunctionFailedNoCode=Falha em %1
ErrorFunctionFailed=Falha em %1; código %2
ErrorFunctionFailedWithMessage=Falha em %1; código %2.%n%3
ErrorExecutingProgram=Não foi possível executar o arquivo:%n%1

; *** Registry errors
ErrorRegOpenKey=Erro ao abrir a chave do Registro:%n%1\%2
ErrorRegCreateKey=Erro ao criar a chave do Registro:%n%1\%2
ErrorRegWriteKey=Erro ao gravar na chave do Registro:%n%1\%2

; *** INI errors
ErrorIniEntry=Erro ao criar a entrada INI no arquivo "%1".

; *** File copying errors
FileAbortRetryIgnoreSkipNotRecommended=&Pular este arquivo (não recomendado)
FileAbortRetryIgnoreIgnoreNotRecommended=&Ignorar o erro e continuar (não recomendado)
SourceIsCorrupted=O arquivo de origem está corrompido
SourceDoesntExist=O arquivo de origem "%1" não existe
SourceVerificationFailed=Falha na verificação do arquivo de origem: %1
VerificationSignatureDoesntExist=O arquivo de assinatura "%1" não existe
VerificationSignatureInvalid=O arquivo de assinatura "%1" é inválido
VerificationKeyNotFound=O arquivo de assinatura "%1" usa uma chave desconhecida
VerificationFileNameIncorrect=O nome do arquivo está incorreto
VerificationFileTagIncorrect=A marca do arquivo está incorreta
VerificationFileSizeIncorrect=O tamanho do arquivo está incorreto
VerificationFileHashIncorrect=O hash do arquivo está incorreto
ExistingFileReadOnly2=Não foi possível substituir o arquivo existente porque ele é somente leitura.
ExistingFileReadOnlyRetry=&Remover o atributo somente leitura e tentar novamente
ExistingFileReadOnlyKeepExisting=&Manter o arquivo existente
ErrorReadingExistingDest=Erro ao ler o arquivo existente:
FileExistsSelectAction=Selecione uma ação
FileExists2=O arquivo já existe.
FileExistsOverwriteExisting=&Substituir o arquivo existente
FileExistsKeepExisting=&Manter o arquivo existente
FileExistsOverwriteOrKeepAll=&Fazer o mesmo nos próximos conflitos
ExistingFileNewerSelectAction=Selecione uma ação
ExistingFileNewer2=O arquivo existente é mais recente do que o que está sendo instalado.
ExistingFileNewerOverwriteExisting=&Substituir o arquivo existente
ExistingFileNewerKeepExisting=&Manter o arquivo existente (recomendado)
ExistingFileNewerOverwriteOrKeepAll=&Fazer o mesmo nos próximos conflitos
ErrorChangingAttr=Erro ao alterar os atributos do arquivo existente:
ErrorCreatingTemp=Erro ao criar um arquivo na pasta de destino:
ErrorReadingSource=Erro ao ler o arquivo de origem:
ErrorCopying=Erro ao copiar um arquivo:
ErrorDownloading=Erro ao baixar um arquivo:
ErrorExtracting=Erro ao extrair um arquivo compactado:
ErrorReplacingExistingFile=Erro ao substituir o arquivo existente:
ErrorRestartReplace=Falha em RestartReplace:
ErrorRenamingTemp=Erro ao renomear um arquivo na pasta de destino:
ErrorRegisterServer=Não foi possível registrar a DLL/OCX: %1
ErrorRegSvr32Failed=O RegSvr32 falhou com o código de saída %1
ErrorRegisterTypeLib=Não foi possível registrar a biblioteca de tipos: %1

; *** Uninstall display name markings
UninstallDisplayNameMark=%1 (%2)
UninstallDisplayNameMarks=%1 (%2, %3)
UninstallDisplayNameMark32Bit=32 bits
UninstallDisplayNameMark64Bit=64 bits
UninstallDisplayNameMarkAllUsers=Todos os usuários
UninstallDisplayNameMarkCurrentUser=Usuário atual

; *** Post-installation errors
ErrorOpeningReadme=Erro ao abrir o arquivo LEIA-ME.
ErrorRestartingComputer=Não foi possível reiniciar o computador. Reinicie-o manualmente.

; *** Uninstaller messages
UninstallNotFound=O arquivo "%1" não existe. Não é possível desinstalar.
UninstallOpenError=Não foi possível abrir o arquivo "%1". Não é possível desinstalar
UninstallUnsupportedVer=O arquivo de log de desinstalação "%1" está em um formato que esta versão do desinstalador não reconhece. Não é possível desinstalar
UninstallUnknownEntry=Uma entrada desconhecida (%1) foi encontrada no log de desinstalação
ConfirmUninstall=Tem certeza de que deseja remover completamente o %1 e todos os seus componentes?
UninstallOnlyOnWin64=Esta instalação só pode ser desinstalada no Windows de 64 bits.
OnlyAdminCanUninstall=Esta instalação só pode ser desinstalada por um usuário com privilégios de administrador.
UninstallStatusLabel=Aguarde enquanto o %1 é removido do seu computador.
UninstalledAll=O %1 foi removido do seu computador.
UninstalledMost=A desinstalação do %1 foi concluída.%n%nAlguns itens não puderam ser removidos e podem ser removidos manualmente.
UninstalledAndNeedsRestart=Para concluir a desinstalação do %1, é necessário reiniciar o computador.%n%nDeseja reiniciar agora?
UninstallDataCorrupted=O arquivo "%1" está corrompido. Não é possível desinstalar

; *** Uninstallation phase messages
ConfirmDeleteSharedFileTitle=Remover Arquivo Compartilhado?
ConfirmDeleteSharedFile2=O sistema indica que o arquivo compartilhado a seguir não é mais usado por nenhum programa. Deseja que a desinstalação remova este arquivo compartilhado?%n%nSe algum programa ainda usar este arquivo e ele for removido, esse programa poderá não funcionar corretamente. Se não tiver certeza, escolha Não. Manter o arquivo no sistema não causará nenhum problema.
SharedFileNameLabel=Nome do arquivo:
SharedFileLocationLabel=Local:
WizardUninstalling=Status da Desinstalação
StatusUninstalling=Desinstalando o %1...

; *** Shutdown block reasons
ShutdownBlockReasonInstallingApp=Instalando o %1.
ShutdownBlockReasonUninstallingApp=Desinstalando o %1.

[CustomMessages]

NameAndVersion=%1 versão %2
AdditionalIcons=Atalhos adicionais:
CreateDesktopIcon=Criar um atalho na &área de trabalho
CreateQuickLaunchIcon=Criar um atalho na barra de &Inicialização Rápida
ProgramOnTheWeb=%1 na Web
UninstallProgram=Desinstalar o %1
LaunchProgram=Abrir o %1
AssocFileExtension=&Associar o %1 à extensão de arquivo %2
AssocingFileExtension=Associando o %1 à extensão de arquivo %2...
AutoStartProgramGroupDescription=Inicialização:
AutoStartProgram=Iniciar o %1 automaticamente
AddonHostProgramNotFound=O %1 não foi encontrado na pasta selecionada.%n%nDeseja continuar mesmo assim?
