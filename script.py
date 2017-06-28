import time
import clipper_admin.clipper_manager
import json
import requests
import test_util as tu


def predict(x):
    url = "http://%s:1337/%s/predict" % ("localhost", app_name)
    req_json = json.dumps({'uid': 0, 'input': list(x)})
    headers = {'Content-type': 'application/json'}
    r = requests.post(url, headers=headers, data=req_json)
    return r.text


c = clipper_admin.clipper_manager.Clipper("localhost")
c.start()

app_name = "app_name"
default_output = "not dog"
model_name = "predicts_dog"
slo_micros = 20000
input_type = "doubles"
model_version = 13
datapoint = [1.0, 2.0]

func = lambda x: [tu.PREDICTION for y in x]

c.register_application(app_name, model_name, input_type, default_output,
                       slo_micros)

c.deploy_predict_function(model_name, model_version, func, input_type)

time.sleep(3)
prediction = predict(datapoint)


# mda = ModuleDependencyAnalyzer()
# # preinstalled_modules = [name for name, _ in self.preinstalled_modules]
# # self._modulemgr.ignore(clipper_admin.cloudpickle)

# from cStringIO import StringIO

# s = StringIO()
# cp = clipper_admin.clipper_manager.cloudpickler(s, 2)
# cp.dump(func)

# for module in cp.modules:
#     mda.add(module.__name__)
"""
With:
from test_util import PREDICTION
func = lambda x: [PREDICTION for y in x]

We get:
u'{"query_id":3,"output":"dog","default":false}'
"""
# When we import some variable directly, it seems to work
"""
With:
import test_util
func = lambda x: [test_util.PREDICTION for y in x]

We get:

No dependencies supplied. Attempting to run Python container.
Starting PythonContainer container
Connecting to Clipper with default port: 7000
Initializing Python function container
No module named test_util
Encountered an ImportError when running Python container.
Please supply necessary dependencies through an Anaconda environment and try again.
"""
# So clearly we need to supply `test_util` and have it be importable
"""
>>> import test_util
>>> import sys
>>> set(sys.modules) & set(globals())
set(['sys', 'test_util'])
"""
"""
>>> import test_util as tu
>>> import sys
>>> set(sys.modules) & set(globals())
set(['sys'])
>>> globals()
{'__builtins__': <module '__builtin__' (built-in)>, 'tu': <module 'test_util' from 'test_util.pyc'>, '__package__': None, 'sys': <module 'sys' (built-in)>, '__name__': '__main__', '__doc__': None}
"""
# because if you do an 'import x as y', then y shows up in globals() but as a key but x shows up in sys.modules as a key
# looks like we really want the stuff in globals

#But the problem is that ModuleDependencyAnalyzer doesn't take things straight from globals. It wants reall names
"""
>>> import test_util as tu
>>> import sys
>>> set(sys.modules) & set(globals())
set(['sys'])
>>> globals()
{'__builtins__': <module '__builtin__' (built-in)>, 'tu': <module 'test_util' from 'test_util.pyc'>, '__package__': None, 'sys': <module 'sys' (built-in)>, '__name__': '__main__', '__doc__': None}
>>> mda.add('tu')
>>> mda.get_and_clear_paths()
set([])
>>> mda.add('test_util')
>>> mda.get_and_clear_paths()
set(['test_util.py'])
"""
"""
>>> from test_util import PREDICTION
>>> import sys
>>> sys.modules['test_util']
<module 'test_util' from 'test_util.pyc'>
>>> globals()
{'__builtins__': <module '__builtin__' (built-in)>, 'PREDICTION': 'dog', '__package__': None, 'sys': <module 'sys' (built-in)>, '__name__': '__main__', '__doc__': None}
"""
# So it looks like if we want full information, we want to capture things in sys.modules

# 1. Run globals() (although if they import from within the ffffff       function you would need to get locals() too)
# 2. Filter out entries whose keys are of the form __x__
# 3. Filter out entries whose values are not modules
# 4. For the remaining modules, grab their mod.__name__s
# 5. Add each to a ModuleDependencyAnalyzer instance
# 6. Retrieve paths with mda.get_and_clear_paths()
# 7. Copy all those files into /model/modules
# 8. 
