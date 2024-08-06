from SCons.Script import DefaultEnvironment
import os

env = DefaultEnvironment()

# Define build command using Makefile
def custom_build(source, target, env):
    # Change directory to the project root
    os.chdir(env.subst('$PROJECT_DIR'))
    os.system('make -C .')

# Register the custom build command
env.AddPreAction("buildprog", custom_build)