
#import yaml
import os
working_dir = os.path.split(__file__)[0]
os.chdir(working_dir)
print(os.getcwd())
print(os.listdir())