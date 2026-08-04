Summary:

I am building a wearable that I will use at the gym to not only track reps, but give a score for form on new exercises using machine learning to detect bad habits in form and compare different exercises at the gym (eg: hammer curl vs bicep curl) to see which activates your muscles the strongest (using emg)

Controllers Used:
- BNO08X (Accel, Gyro, Orientation)
- Myoware Muscle Sensor 2.0 (EMG from muscles)
- LilyGO T-Display S3 (Esp32 + display)

Process
- Creating Device (Case, Wiring, Software) ⏳
  - Eventually get a second IMU / EMG
- Collecting data + modeling (for machine learning detection ) ❌
  - Each exercise will have different places for the wearable to maximize information (eg, thigh for squat)
- Integrating Model + bluetooth support to get data ❌
--------------------------------------------------------------------------------------------
Why:

I've been going to the gym for almost a year now and I have been finding out new information every day: how many excersies you should do, where they should go, what you should eat after, what muscle groups to hit, an most importantly, the different forms and variations to exercises. I would always ask AI what I should feel and search up videos only to get conflicting information on what form is wrong. I wanted to make this device specifically since I have a super tight ankle and have to put plates below my feet for squats and it messes with my form, I want to have something easy that immediately understands me no matter where I am.

What I want for this device is to provide suggestions improve muscle activation and growth with specific tips in specific excerises (and other suggestions for exercises to find more that they may enjoy better), and also give feedback on form for a good group exercises with specific feedback (eg for squats, too much back activation / recommend X, or too much momentum). During this I also want to emphasize to the user that its not just muscle activation, but mind muscle connection, risk (e.g. preacher curls), form, and a strong stretch to maximize growth. My greatest goal for this is to make it personalized for each person rather than general data too, however I am sitll braintsomrning how to personalize models / machine learning to each person and relying on data we all have in common

I hope to present this to my school to get into the engineering two honors class, and maybe even eventually a fair to show off the practicality of a wearable like this with muscle activation rating. Additionally, in the future I want to make the main wearable more of a hub and make nodes with small microcontrollers that have IMU's and EMG's on them to have more data from other body parts to more fully check if an exercise is activating other muscles and check other parts of form.

