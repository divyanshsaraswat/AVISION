import torch
import torch.nn as nn
import torch.onnx
import sys
import os

# ==============================================================================
# INSTRUCTIONS
# ==============================================================================
# 1. Install dependencies: pip install torch torchvision onnx
# 2. Define or Import your model class below.
# 3. Set the 'MODEL_PATH' to your .pth file.
# 4. Run: python convert_to_onnx.py
# 4. Run: python convert_to_onnx.py
# ==============================================================================

import urllib.request

def download_file(url, filename):
    if not os.path.exists(filename):
        print(f"Downloading {filename} from {url}...")
        urllib.request.urlretrieve(url, filename)
        print("Download complete.")
    else:
        print(f"File {filename} already exists.")

def convert_model():
    # --- CONFIGURATION (EDIT THESE) ---
    # --- CONFIGURATION ---
    input_model_path = "resnet18_places365.pth.tar"   # Note: Official MIT file is .tar
    output_onnx_path = "resnet18_places365.onnx"
    input_shape = (1, 3, 224, 224)
    
    # --- DEFINE MODEL ---
    from torchvision import models
    print("Creating ResNet18 model instance...")
    model = models.resnet18(num_classes=365)
    
    # --- LOAD WEIGHTS ---
    # Official MIT Places365 ResNet18
    # URL: http://places2.csail.mit.edu/models_places365/resnet18_places365.pth.tar
    pth_url = "http://places2.csail.mit.edu/models_places365/resnet18_places365.pth.tar"
    download_file(pth_url, input_model_path)

    print(f"Loading weights from {input_model_path}...")
    if os.path.exists(input_model_path):
        try:
            # Note: The usage of .pth.tar often implies it contains a dict with 'state_dict' key
            checkpoint = torch.load(input_model_path, map_location='cpu')
            
            # Extract state_dict if it's nested
            if 'state_dict' in checkpoint:
                state_dict = checkpoint['state_dict']
            else:
                state_dict = checkpoint

            # Fix keys: "module." prefix from DataParallel, and "fc" renaming
            new_state_dict = {}
            for k, v in state_dict.items():
                name = k
                if name.startswith('module.'):
                    name = name[7:]
                new_state_dict[name] = v
                
            missing, unexpected = model.load_state_dict(new_state_dict, strict=False)
            print("Weights loaded.")
            print(f"Missing keys: {len(missing)}")
            print(f"Unexpected keys: {len(unexpected)}")
            if len(missing) > 0: print(f"Sample Missing: {missing[:3]}")
            
        except Exception as e:
            print(f"Error loading weights: {e}")
            return
    else:
        print("Model file not found.")
        return

    model.eval()

    # --- EXPORT TO ONNX ---
    print(f"Exporting to {output_onnx_path}...")
    dummy_input = torch.randn(*input_shape)
    
    torch.onnx.export(
        model, 
        dummy_input, 
        output_onnx_path,
        verbose=False,
        input_names=['input'], 
        output_names=['output'],
        opset_version=11
    )
    
    print("Conversion complete!")

if __name__ == "__main__":
    convert_model()
