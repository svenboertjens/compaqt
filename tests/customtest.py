import compaqt

encode = compaqt.encode
decode = compaqt.decode
validate = compaqt.validate

class Test():
    def __init__(self):
        self.fails = 0
        self.failed = []
    
    def test(self, value):
        encoded = encode(value)
        
        if decode(encoded) != value:
            strval = str(value)
            
            if len(strval) > 64:
                strval = strval[:64] + " ... (cut off)"
            
            self.failed.append(strval)
            self.fails += 1
        elif not validate(encoded):
            print("Incorrectly invalidated!")
    
    def finalize(self):
        if self.fails == 0:
            print("No issues found!")
        else:
            print("Failed for values:")
            for strval in self.failed:
                print(strval)
            
            print(f"\nFailed {self.fails} times.")
        
        self.fails = 0
        self.failed = []
    
    def test_values(self, values):
        for value in values:
            self.test(value)
        self.finalize()
