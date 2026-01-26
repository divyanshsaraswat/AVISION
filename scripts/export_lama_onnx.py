import os
import sys
import argparse
import torch
import yaml
from omegaconf import OmegaConf

# NOTE: This script assumes you are running it from the root of the 'lama' repository
# and have the environment installed (hydra, pytorch_lightning, etc.)

try:
    from saicinpainting.training.trainers import load_checkpoint
except ImportError:
    print("Error: Could not import 'saicinpainting'. Make sure you are in the 'lama' repository root and the environment is active.")
    sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="Export Big-LaMa .ckpt to .onnx")
    parser.add_argument("checkpoint", help="Path to best.ckpt")
    parser.add_argument("config", help="Path to config.yaml (usually in the same folder as ckpt or configs/prediction/default.yaml)")
    parser.add_argument("--output", default="big_lama.onnx", help="Output ONNX file path")
    parser.add_argument("--size", type=int, default=512, help="Input resolution (default: 512)")
    
    args = parser.parse_args()

    device = torch.device('cpu') # Exporting on CPU is safer for portability

    # 1. Load Configuration
    print(f"Loading config from {args.config}...")
    train_config = OmegaConf.load(args.config)
    
    # Enable predictive mode keys if missing
    train_config.training_model.predict_only = True
    train_config.visualizer.kind = 'noop'

    # 2. Load Model
    print(f"Loading checkpoint from {args.checkpoint}...")
    model = load_checkpoint(train_config, args.checkpoint, strict=False, map_location='cpu')
    model.to(device)
    model.eval()
    model.freeze()

    # 3. Prepare Dummy Inputs
    # LaMa expects: image (B, 3, H, W) and mask (B, 1, H, W)
    # Both normalized typically? The model usually handles raw input normalization if configured, 
    # but strictly speaking, standard LaMa input is:
    # Image: [0, 1] range float32
    # Mask: [0, 1] range float32 (1=hole, 0=valid)
    
    dummy_image = torch.randn(1, 3, args.size, args.size, device=device)
    dummy_mask = torch.randint(0, 2, (1, 1, args.size, args.size), device=device).float()
    
    dummy_inputs = (
        {
            'image': dummy_image,
            'mask': dummy_mask
        }
    )

    # Wrapper to handle dictionary input for ONNX export (Tracing often prefers tuples)
    # We might need to wrap the forward method if it strictly expects a dict
    class OnnxWrapper(torch.nn.Module):
        def __init__(self, model):
            super().__init__()
            self.model = model

        def forward(self, image, mask):
            # Construct dict expected by LaMa
            batch = {'image': image, 'mask': mask}
            result = self.model(batch)
            # Result is usually a dict or image. LaMa generator returns 'predicted_image'.
            return result['predicted_image']

    wrapped_model = OnnxWrapper(model)

    # 4. Export
    print(f"Exporting to {args.output}...")
    
    input_names = ["image", "mask"]
    output_names = ["output_image"]
    
    # Dynamic axes allow variable resolution at runtime (optional, but recommended for cropping)
    dynamic_axes = {
        "image": {2: "height", 3: "width"},
        "mask": {2: "height", 3: "width"},
        "output_image": {2: "height", 3: "width"}
    }

    torch.onnx.export(
        wrapped_model,
        (dummy_image, dummy_mask),
        args.output,
        export_params=True,
        opset_version=16, # High opset for safe FFT support
        do_constant_folding=True,
        input_names=input_names,
        output_names=output_names,
        dynamic_axes=dynamic_axes
    )

    print("Success! Post-export check (requires onnx)...")
    try:
        import onnx
        onnx_model = onnx.load(args.output)
        onnx.checker.check_model(onnx_model)
        print("ONNX model is valid.")
    except ImportError:
        print("Skipping check (onnx module not installed).")

if __name__ == "__main__":
    main()
