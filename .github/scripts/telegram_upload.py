import json
import os
import subprocess
import sys

zip_path = os.environ.get("ZIP_PATH")
if not zip_path or not os.path.exists(zip_path):
    sys.exit(f"ERROR: The file '{zip_path}' DOES NOT EXIST.")

token = os.environ["BOT_TOKEN"]
chat_id = os.environ["CHAT_ID"]
topic_id = os.environ.get("TOPIC_ID", "")
msg = os.environ.get("COMMIT_MESSAGE", "Manual build dispatch").split('\n')[0].strip()
commit_url = os.environ.get("COMMIT_URL", f"https://github.com/{os.environ['GITHUB_REPOSITORY']}")
run_url = os.environ["RUN_URL"]
run_number = os.environ["RUN_NUMBER"]
branch = os.environ.get("GITHUB_REF_NAME", "unknown")

MD_CHARS = '_*[]()~`>#+-=|{}.!'
ESCAPE_MD_MAP = {ord(c): f'\\{c}' for c in MD_CHARS}
ESCAPE_CODE_MAP = {ord('\\'): '\\\\', ord('`'): '\\`'}

title = f"NoMount CI Build ({branch} branch)"
run_text = f"\\#ci\\_{run_number}"

escaped_title = title.translate(ESCAPE_MD_MAP)
escaped_msg = msg.translate(ESCAPE_CODE_MAP)

caption = f"*{escaped_title}*\n{run_text}\n```text\n{escaped_msg}\n```\n[Commit]({commit_url}) \\| [Workflow]({run_url})"
if len(caption) > 1024:
    caption = caption[:1015] + "...\n```"

url_telegram = f"https://api.telegram.org/bot{token}/sendDocument"

curl_cmd = [
    "curl", "-sS", "-X", "POST",
    url_telegram,
    "-F", f"chat_id={chat_id}",
    "-F", f"caption={caption}",
    "-F", "parse_mode=MarkdownV2",
    "-F", f"document=@{zip_path}"
]

if topic_id:
    curl_cmd.extend(["-F", f"message_thread_id={topic_id}"])

print(f"Uploading {zip_path} to Telegram...")
result = subprocess.run(curl_cmd, capture_output=True, text=True)

if result.returncode != 0:
    sys.exit(f"Critical error: cURL failed (Exit code {result.returncode})\n{result.stderr}")

try:
    response = json.loads(result.stdout)
    if not response.get("ok"):
        sys.exit(f"Error returned by Telegram API:\n{json.dumps(response, indent=2)}")
    print("Artifact successfully uploaded to Telegram!")
except json.JSONDecodeError:
    sys.exit(f"Telegram response is not valid JSON:\n{result.stdout}")
