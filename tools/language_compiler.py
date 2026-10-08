import os
import polib

print("Compiling all languages...")

# The locale folder sits at the root of the project, one level above this one.
LOCALE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'locale')

for root, dirs, files in os.walk(LOCALE):
    for file in files:
        if file.endswith('.po'):
            po_path = os.path.join(root, file)
            mo_path = os.path.splitext(po_path)[0] + '.mo'

            print(f"Compiling: {po_path} -> {mo_path}")

            po = polib.pofile(po_path)
            po.save_as_mofile(mo_path)

print("All translations were compiled successfully!")
input("Press ENTER to exit...")
