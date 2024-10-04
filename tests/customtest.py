import compaqt

encode = compaqt.encode
decode = compaqt.decode
validate = compaqt.validate

class Test():
    def __init__(self):
        self.failed = []
        self.other = []
    
    def shortval(self, value):
        strval = str(value)
        
        if len(strval) > 64:
            strval = strval[:64] + " ... (cut off)"
        
        return strval
    
    def test(self, value):
        encoded = encode(value)
        
        try:
            if decode(encoded) != value:
                strval = self.shortval(value)
                
                self.failed.append(strval)
                
                if validate(encoded):
                    self.other.append((strval, "Deemed valid while value was not correct"))
                
            elif not validate(encoded):
                self.other.append((self.shortval(value), "Deemed invalid while value was correct"))
        
        except Exception as e:
            self.other.append((self.shortval(value), f"Received exception: {e}"))
    
    def finalize(self):
        if len(self.failed) + len(self.other) == 0:
            print("No issues found!")
        else:
            if len(self.failed) != 0:
                print("Failed for values:")
                print(''.join(value + '\n' for value in self.failed))
            
            if len(self.other) != 0:
                print("Got messages for values:")
                print(''.join([f"{value}: {message}\n" for value, message in self.other]))
        
        self.failed = []
        self.other = []
    
    def test_values(self, values):
        for value in values:
            self.test(value)
        self.finalize()
