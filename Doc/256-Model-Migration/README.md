# 256 Model Migration Handover

## Scope

This note summarizes the current state of the `256x256` MoveNet migration on `STM32N6570-DK` so another agent can continue without replaying the whole investigation.

Current target model:

- `Model/st_movenet_lightning_a100_heatmaps_256_int8.tflite`

Also tested:

- `Model/st_movenet_lightning_a100_heatmaps_256_int8_per_tensor.tflite`

## Current Status

### Known-good baseline

The stock prebuilt image works on hardware:

- `Binary/STM32N6570-DK/STM32N6570-DK_GettingStarted_PoseEstimation.hex`

Observed with the stock image:

- LCD boots normally
- camera stream works
- pose inference works
- user reported roughly `18 ms` inference

This proves the following are good:

- board hardware
- FSBL path
- boot-from-flash path
- display path
- camera path
- stock model/runtime integration

### Current 256 state

The custom `256 int8` build now boots from flash, but inference is still abnormal:

- screen comes up
- app runs
- reported inference time is around `451 ms`
- inference result quality is poor
- keypoints tend to collapse to one point
- user reported `Raw[-128,-12] Deq[0.00,0.02]`

This means the original boot/signing issue is fixed, but the `256` runtime behavior is still wrong or heavily suboptimal.

## Most Important Finding

### Flash boot failure root cause was signing alignment

For a long time, custom signed images showed only a black screen even though flashing succeeded.

The root cause was the signing command in:

- `Application/STM32N6570-DK/Makefile`

The fix was adding `-align` to the signing step:

```bash
STM32_SigningTool_CLI ... -hv 2.3 -align
```

Without `-align`:

- custom `Project_sign.bin` did not match the layout expected by FSBL
- vector table was not aligned like the working prebuilt image
- flash boot resulted in a black screen

With `-align`:

- custom app boots from flash again

This is already committed in:

- commit `c23fc8b`

## Files Currently Modified

### Application changes

- `Application/STM32N6570-DK/Makefile`
  - signing command includes `-align`

- `Application/STM32N6570-DK/Inc/app_config.h`
  - switched to 17 keypoints
  - updated skeleton bindings for COCO-style 17-keypoint MoveNet
  - `WELCOME_MSG_1` changed to the 256 model name
  - `ASPECT_RATIO_MODE` changed to `ASPECT_RATIO_FIT`
  - `AI_POSE_PP_CONF_THRESHOLD` reduced to `0.005f`

- `Application/STM32N6570-DK/Src/main.c`
  - added runtime overlay diagnostics
  - added output-cache invalidate before postprocess
  - `printf` already routed to `USART1` at `115200`

- `Middlewares/ai-postprocessing-wrapper/app_postprocess_spe_movenet_ui.c`
  - modified to test both `HWC` and `CHW` output layout decoding and pick the more plausible one

### Model generation changes

- `Model/generate-n6-model_STM32N6570-DK.sh`
  - currently points back to:
    - `st_movenet_lightning_a100_heatmaps_256_int8.tflite`

- `Model/user_neuralart_STM32N6570-DK.json`
  - reduced options:
    - `--all-buffers-info --cache-maintenance --Ocache-opt --Os`

- generated model files under:
  - `Model/STM32N6570-DK/`

## Board Measurements And Observations

### 192 stock prebuilt

User-observed:

- inference around `18 ms`
- pose detection works

### 256 int8 per-channel

Generation report:

- model format: `per channel`
- weights: about `2.64 MiB`
- activations: about `2.75 MiB`

Board behavior:

- boots successfully after `-align` fix
- reported inference around `451 ms`
- output quality poor
- keypoints collapse toward one location
- dequantized output range stays very low

### 256 int8 per-tensor

Generation report:

- model format: `per tensor`
- weights: about `2.45 MiB`
- activations: about `6.25 MiB`

Board behavior:

- extremely slow
- user observed around `751 ms`

Important interpretation:

- `per_tensor` was not faster here
- on this project/toolchain/board combination it was much worse
- generation report showed many software fallback epochs

## Why The Current 451 ms State Looks Wrong

The current `451 ms` is not explained by `192 -> 256` input growth alone.

Expected from ModelZoo reference:

- `192 int8`: about `22.05 ms`
- `224 int8`: about `27.64 ms`
- `256 int8`: about `35.50 ms`

Reference source:

- `/home/min/Workspace/Graduate-Project/stm32ai-modelzoo/pose_estimation/movenet/README.md`

So the current `451 ms` state is abnormal by about an order of magnitude.

Likely remaining causes:

- camera/input preprocessing mismatch versus the model's expected input
- postprocess still interpreting output incorrectly
- hidden software fallback or memory traffic not reflected in the simple summary
- cache coherency issue not fully solved
- runtime integration differs from the ModelZoo reference application

## Runtime Diagnostics Added

Current on-screen overlay now includes:

- `Inference`
- `KP x/17 Max y.yy`
- `Raw[min,max] Deq[min,max]`
- `Out WxHxC PP ret`

These were added in:

- `Application/STM32N6570-DK/Src/main.c`

Purpose:

- distinguish "model is running but low-confidence" from "postprocess is broken"
- expose whether output tensor values are saturated or nearly flat
- expose whether postprocess returns an error

## Logging Status

The firmware already supports `printf` over UART:

- `Application/STM32N6570-DK/Src/main.c`
  - `CONSOLE_Config()` sets up `USART1` on `PE5/PE6` at `115200`
  - `_write()` transmits over `HAL_UART_Transmit(&huart1, ...)`

Current blocker on host side:

- ST-LINK is detected over USB
- but no `/dev/ttyACM*` device is currently visible from this PC

So firmware-side logging path exists, but host-side serial capture is not currently available.

## Flashing And Boot Procedure That Works

### External flash addresses

- `0x70000000`: `FSBL/ai_fsbl.hex`
- `0x70100000`: signed app
- `0x70380000`: model weights

### Working sequence

1. Put board in `Development Mode`
2. Program:
   - `FSBL/ai_fsbl.hex`
   - `Application/STM32N6570-DK/build/Application/STM32N6570-DK/Project_sign.bin`
   - `Model/STM32N6570-DK/network_data.hex`
3. Switch to `Boot from Flash`
4. Power cycle

Important:

- use `Project_sign.bin`, not raw `Project.bin`
- signed image must be produced with `-align`

## What Was Already Ruled Out

- board hardware failure
- generic flash write failure
- missing FSBL
- generic boot-from-flash failure
- `17 keypoints` being the primary black-screen cause
- per-tensor being automatically faster

Reason:

- stock `192` prebuilt works
- custom image boots after signing fix
- "model-only" and "postprocess-only" changes were separated during debugging

## Open Questions

1. Why does the same `256 int8` family that ModelZoo reports near `35 ms` run at about `451 ms` here?
2. Is the camera preprocessing path feeding the model in the exact format expected by this `256` model?
3. Is `ASPECT_RATIO_FIT` correct for this model, or should the pipeline match the ModelZoo reference more closely?
4. Is the custom dual-layout postprocess masking a deeper output-format mismatch?
5. Are there still runtime/library/config differences versus the ModelZoo STM32N6570-DK reference app?

## Best Next Steps

### Highest-value investigation

1. Compare this application's preprocessing path against the ModelZoo MoveNet N6 reference.
2. Add serial logs for:
   - NN input stats
   - output stats
   - postprocess selected layout
   - per-frame timings around capture, inference, and postprocess
3. Verify whether `USART1` can be captured through ST-LINK VCP on the host.
4. Compare generated `256` network integration against the ModelZoo reference project, not just the model file.

### If another agent continues code work

Start from the current `256 int8` per-channel state, not the `per_tensor` state.

The current useful branch of investigation is:

- fix the `451 ms` abnormal runtime
- fix incorrect pose output quality

The boot/signing problem is already solved.

## Handy References

- baseline prebuilt:
  - `Binary/STM32N6570-DK/STM32N6570-DK_GettingStarted_PoseEstimation.hex`
- app config:
  - `Application/STM32N6570-DK/Inc/app_config.h`
- main loop and diagnostics:
  - `Application/STM32N6570-DK/Src/main.c`
- MoveNet postprocess wrapper:
  - `Middlewares/ai-postprocessing-wrapper/app_postprocess_spe_movenet_ui.c`
- signing fix:
  - `Application/STM32N6570-DK/Makefile`
- model generation script:
  - `Model/generate-n6-model_STM32N6570-DK.sh`
- generation report:
  - `Model/st_ai_output/network_generate_report.txt`
