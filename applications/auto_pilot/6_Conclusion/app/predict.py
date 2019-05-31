
def predict(info):
    
    try:   
        print("Received", info)
        return info

    except Exception as exc:
        print('Generated an exception: %s' % (exc))

