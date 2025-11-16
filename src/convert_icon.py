from PIL import Image
import subprocess

def create_icon(source_path, output_path, icon_sizes=(16, 20, 24, 32, 48, 64, 128, 256, 512)):
    image = Image.open(source_path)
    icon_sizes = [(size, size) for size in icon_sizes]
    image.save(output_path, format='ICO', sizes=icon_sizes)

source = '../assets/icon.png'
output = '../assets/icon.ico'
create_icon(source, output)

subprocess.run(['windres', 'assets/resources.rc', '-O', 'coff', '-o', 'assets/icon.res'])
