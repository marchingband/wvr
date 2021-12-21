const char RECOVERY_HTML[] = R"=====(
<!doctype html>
<html lang="en">
    <head>
        <title>Waver Recovery UI</title>
    </head>
    <body>
        <p>Choose a firmware (.bin) and click submit. It will upload and reboot into it</p>
        <p>Wait about 30 seconds for the process to complete, then refresh the browser</p>
        <form id="firmware">
          <input type="file" id="firmwareFileInput" name="filename">
        </form>
        <button onclick="upload()" id="firmwareFileButton">Upload</button>
        <p style="margin-top: 100px; color:red">Click RESET EMMC to reset the eMMC memory on WVR. All your sounds, firmwares, and configuration will be deleted</p>
        <button id="emmcReset">RESET EMMC</button>
    </body>
    <script>

        const upload = async() => {
            const firmware = document.getElementById('firmwareFileInput').files[0];
            if(!firmware){
                window.alert("no file selected");
                return;
            }
            console.log(firmware);
            window.alert("Click OK to start upload, you will be alerted when the process is complete, plese wait")
            await fetch(
                // "/update",
                "http://192.168.5.18/update",
                {
                    method:"POST",
                    body:firmware,
                    headers: {
                        'Content-Type': 'text/html',
                    }
                }
            );
            window.alert("DONE! please refresh the browser now")
        }

        const emmcResetButton = document.getElementById('emmcReset');
        emmcResetButton.addEventListener('click', async _ => {
            try {  
                if(!window.confirm('Are you sure you want to reset the eMMC memory?')){
                    return;
                }  
                const response = await fetch('http://192.168.5.18/emmcReset', {
                    method: 'get',
                });
                console.log('Completed!', response);
            } catch(err) {
                console.error(`Error: ${err}`);
            }
        });
    </script>
</html>
)=====";