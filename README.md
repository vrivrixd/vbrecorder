# VB Recorder

Grave qualquer áudio do Windows, de múltiplas fontes ao mesmo tempo, sem cabos
virtuais, drivers extras ou configurações complicadas de roteamento.

O VB Recorder captura áudio de microfones, teclados, mesas e placas de som, e
também de processos em execução, como navegadores, players de música e
aplicativos de chamada. Tudo junto, com controle de volume individual para cada
fonte, e com pouquíssimo atraso: dá para gravar sua performance por cima de um
playback.

O programa é leve, portátil e roda na bandeja do sistema.

## Recursos

- **Múltiplas fontes simultâneas.** Adicione quantas fontes quiser à lista de
  gravação, misturando dispositivos de áudio e processos.
- **Captura por processo.** Usando a API de captura por processo do Windows
  (WASAPI), o áudio de um programa específico é capturado direto, sem cabo
  virtual.
- **Volume por fonte.** Ajuste de -20 dB a +20 dB em cada fonte, para equilibrar
  microfones fracos sem afetar as demais.
- **Teste antes de gravar.** O botão Testar deixa você ouvir o dispositivo com o
  volume já configurado.
- **Arquivos separados.** Com duas ou mais fontes, cada uma pode ir para o seu
  próprio arquivo, nomeado pela fonte: `gravação (Microfone).wav`,
  `gravação (chrome.exe).wav`.
- **Reproduzir e excluir.** Ouça a última gravação e apague se não tiver ficado
  boa, pelo próprio programa ou pelo menu da bandeja.
- **Teclas de atalho globais**, configuráveis, que funcionam com a janela
  minimizada ou oculta.
- **Janela de configurações** com idioma, iniciar minimizado, minimizar para a
  bandeja, janela sempre no topo e tamanho do buffer, além das teclas de atalho.

## Formatos

| Formato | Observações                                  |
|---------|----------------------------------------------|
| MP3     | bitrate de 8 a 320 kbps                      |
| WAV     | sem compressão, 16 bits                      |
| AIFF    | sem compressão, 16 bits                      |
| OGG     | bitrate ajustável até 500 kbps               |
| OPUS    | codec moderno de alta eficiência, ideal para voz |
| FLAC    | sem perdas, nível de compressão de 0 a 8     |

Taxa de amostragem de 8000 Hz a 48000 Hz, mono ou estéreo.

## Idiomas

O VB Recorder inicia no idioma da interface do Windows, e você pode escolher
outro em Configurações. Estão disponíveis:

português do Brasil, português de Portugal, inglês, espanhol, francês, alemão,
italiano, russo e vietnamita.

Se o Windows estiver em um idioma sem tradução, o programa aparece em português
do Brasil.

**Quer traduzir para o seu idioma?** Copie `locale/vbrecorder.pot` para
`locale/<código>/LC_MESSAGES/vbrecorder.po`, preencha os textos, rode o
`language_compiler.py` e pronto: nenhuma alteração no código é necessária. O que
ficar sem tradução continua aparecendo em português, então dá para traduzir aos
poucos. Contribuições são bem-vindas por [Pull
Request](https://github.com/vrivrixd/vbrecorder/pulls).

## Requisitos

- Windows 10 ou superior.
- Para captura de processos, Windows 10 versão 2004 (maio de 2020) ou superior.
- Não requer drivers adicionais nem cabos virtuais.

## Instalação

Baixe o `VBRecorder.exe` na página de
[Releases](https://github.com/vrivrixd/vbrecorder/releases) e execute. Não há
instalador: o programa é um único arquivo e cria o `VBRecorder.ini` com as suas
configurações ao lado dele.

## Rodando a partir do código

```
pip install wxPython
python main.py
```

As bibliotecas do BASS já acompanham o repositório, na pasta `lib`. O programa
procura as DLLs ali e, se não encontrar, procura ao lado do próprio programa.

Estrutura:

- `main.py` — ponto de entrada.
- `gui.py` — interface, bandeja do sistema e atalhos globais.
- `audio_engine.py` — gravação, mixagem, codificação e reprodução via BASS.
- `process_audio.py` — captura de áudio por processo (WASAPI).
- `config.py` — leitura e gravação do `VBRecorder.ini`.
- `i18n.py` — escolha do idioma e carregamento das traduções.
- `locale/` — catálogos de tradução, um por idioma.
- `language_compiler.py` — compila os `.po` em `.mo`.

## Licença

O código do VB Recorder é distribuído sob a
[GNU General Public License v3.0](LICENSE).

As bibliotecas da pasta `lib` (BASS e seus complementos) **não** fazem parte
dessa licença. Elas são da [un4seen Developments](https://www.un4seen.com/) e
seguem os termos próprios do BASS, que é gratuito para uso não comercial.

## Autor

VB Recorder foi desenvolvido por **Vitor Bruski**.

### Traduções

- Vietnamita: **nguyenninhhoang**

Dúvidas, sugestões e relatos de problemas são bem-vindos na aba
[Issues](https://github.com/vrivrixd/vbrecorder/issues).
