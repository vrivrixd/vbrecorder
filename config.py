# VB Recorder - Gravador de áudio do Windows
# Copyright (C) 2026 Vitor Bruski
#
# Este programa é software livre: você pode redistribuí-lo e/ou modificá-lo sob
# os termos da GNU General Public License, publicada pela Free Software
# Foundation, na versão 3 da licença ou, a seu critério, em qualquer versão
# posterior.
#
# Este programa é distribuído na esperança de que seja útil, mas SEM NENHUMA
# GARANTIA. Veja a GNU General Public License para mais detalhes.
#
# Uma cópia da licença acompanha este programa, no arquivo LICENSE. Se não,
# veja <https://www.gnu.org/licenses/>.

import configparser
import os

CONFIG_FILE = 'VBRecorder.ini'


class ConfigManager:
    def __init__(self):
        self.config = configparser.ConfigParser()
        self.load()

    def load(self):
        if os.path.exists(CONFIG_FILE):
            self.config.read(CONFIG_FILE)

        if 'CONFIG' not in self.config:
            self.config['CONFIG'] = {
                'formato': 'mp3', 'bitrate': '128 KBPS', 'flac_nivel': '4', 'taxa': '44100',
                'canais': 'stereo', 'fontes': 'device:0:1.0',
                'reproduzir_tons': 'False', 'arquivos_separados': 'False',
                'arquivo': '', 'diretorio': ''
            }
        else:
            if 'fontes' not in self.config['CONFIG']:
                d1 = self.config['CONFIG'].get('dispositivo1', '0')
                self.config['CONFIG']['fontes'] = f'device:{d1}:1.0'

        if 'KEYS' not in self.config:
            self.config['KEYS'] = {
                'gravar': '', 'pausar': '', 'parar': '', 'mostrar': '', 'fechar': ''
            }

    def save(self):
        current = dict(self.config['CONFIG']) if 'CONFIG' in self.config else {}
        ordered = ['formato', 'bitrate', 'flac_nivel', 'taxa', 'canais', 'fontes',
                   'reproduzir_tons', 'arquivos_separados', 'arquivo', 'diretorio']
        self.config['CONFIG'] = {}
        for k in ordered:
            if k in current:
                self.config['CONFIG'][k] = current[k]
        for k, v in current.items():
            if k not in ordered:
                self.config['CONFIG'][k] = v
        with open(CONFIG_FILE, 'w') as f:
            self.config.write(f)

    def set_config(self, key, value):
        self.config['CONFIG'][key] = str(value)

    def set_key(self, key, value):
        self.config['KEYS'][key] = str(value)

    def get(self, key):
        return self.config['CONFIG'].get(key)

    def get_key(self, key):
        return self.config['KEYS'].get(key)

    def getboolean(self, key):
        return self.config['CONFIG'].getboolean(key, fallback=False)

    def get_fontes(self):
        raw = self.config['CONFIG'].get('fontes', 'device:0')
        result = []
        for part in raw.split(','):
            part = part.strip()
            tokens = part.split(':')
            if len(tokens) >= 2:
                kind = tokens[0]
                id_str = tokens[1]
                if id_str.lstrip('-').isdigit():
                    volume = float(tokens[2]) if len(tokens) >= 3 else 1.0
                    result.append({'type': kind, 'id': int(id_str), 'volume': volume})
        return result if result else [{'type': 'device', 'id': 0, 'volume': 1.0}]

    def set_fontes(self, sources):
        self.config['CONFIG']['fontes'] = ','.join(
            f"{s['type']}:{s['id']}:{s.get('volume', 1.0):.4f}" for s in sources)
