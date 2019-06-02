from time import sleep

def predict(input):
	sleep_time = int(input.split("***")[0])
	prevent_cache = input.split("***")[1]
    sleep(sleep_time/1000)
    return input



