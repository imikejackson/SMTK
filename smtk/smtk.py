
def __bootstrap_smtk__():
  import sys, os

  shiboken_lib = "@SHIBOKEN_LIBRARY@"
  shiboken_path = os.path.dirname(shiboken_lib)
  smtk_path = "@LIBRARY_OUTPUT_PATH@"

  extra_paths = [shiboken_path, smtk_path]
  for (path, dirs, files) in os.walk(shiboken_path, topdown=False):
    #walk shiboken path and find the python/site-packages sub folders
    #add this to sys path
    x = [os.path.join(path,dir) for dir in dirs if "site-packages" in dir]
    if len(x) > 0:
      extra_paths = extra_paths + x
  sys.path = sys.path + extra_paths

def __import_shared_ptrs__():
  import re
  extract_name = re.compile("\W+")
  def __make_clean_name(name,obj):

    #known classes with the same name between the modules
    known_clashes = ("Item")

    #split the name so we will have
    #[shared_ptr, smtk, model/atribute, name]
    split_name = [i for i in re.split(extract_name, name)]

    #assign the name
    new_name = split_name[3]
    #if the name is a known clashable name, extract the namespace
    #and make it a title
    if(new_name in known_clashes):
      new_name = split_name[2].title()+new_name

    #assign ptr to the end of the name
    return new_name+"Ptr"

  for i in _temp.__dict__:
    #I can't seem to find the type Shiboken.ObjectType, so we will hope
    #the inverse allways works
    if not isinstance(_temp.__dict__[i],(str)):
      try:
        name = __make_clean_name(i,_temp.__dict__[i])
        #now we are also going to
        globals()[name]  = _temp.__dict__[i]
      except:
        pass

__bootstrap_smtk__()

#import the modules information we need.
#We are using _import__ since shiboken doesn't create proper python modules
#We can't just import SMTKCorePython.smtk.model as model, so instead
#we have to use _import__ so that we get a nice interface.

import shiboken
_temp = __import__('SMTKCorePython', globals(), locals(), [], -1)
__import_shared_ptrs__()

model = _temp.smtk.model
attribute = _temp.smtk.attribute
