class Test():
    def __init__(self):
        self.fails = 0
        self.failed = []
    
    def test(self, value, encode, decode, encode_kwargs={}, decode_kwargs={}):
        if decode(encode(value, **encode_kwargs), **decode_kwargs) != value:
            strval = str(value)
            
            if len(strval) > 64:
                strval = strval[:64] + " ... (cut off)"
            
            self.failed.append(strval)
            self.fails += 1
    
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
    
    def test_values(self, values, encode, decode, encode_kwargs={}, decode_kwargs={}):
        for value in values:
            self.test(value, encode, decode, encode_kwargs, decode_kwargs)
        self.finalize()
