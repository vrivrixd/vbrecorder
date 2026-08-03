import os
import polib

print("Compilando todos os idiomas...")

for root, dirs, files in os.walk('locale'):
    for file in files:
        if file.endswith('.po'):
            po_path = os.path.join(root, file)
            mo_path = os.path.splitext(po_path)[0] + '.mo'
            
            print(f"Compilando: {po_path} -> {mo_path}")
            
            po = polib.pofile(po_path)
            po.save_as_mofile(mo_path)

print("Todas as traduções foram compiladas com sucesso!")
input("Pressione ENTER para sair...")