import importlib
import clipper_admin.cloudpickle as cloudpickle
import os

def load_predict_func(file_path):
    with open(file_path, 'r') as serialized_func_file:
        return cloudpickle.load(serialized_func_file)

def pred(f, path):
	return f.endswith(".py")# and isfile(join(path, f))

def load_modules(path):
    # modules_fnames = [f for f in os.listdir(path) if pred(f, path)]
    import pickle
    with open (path, 'rb') as fp:
    	module_fnames = pickle.load(fp)
    	print(module_fnames)
    	for m in module_fnames:
    		globals()[m] = importlib.import_module(m)

load_modules("/tmp/predict_serializations/predicts_dog/modules.txt")
f = load_predict_func("/tmp/predict_serializations/predicts_dog/predict_func.pkl")
f([3])