const copyButton = document.querySelector('#copy-command');
const command = `git clone https://github.com/drmbios/pydev.git\ncd pydev\nmake && make check`;

copyButton.addEventListener('click', async () => {
  try {
    await navigator.clipboard.writeText(command);
    copyButton.textContent = 'COPIED ✓';
    window.setTimeout(() => { copyButton.textContent = 'COPY'; }, 1800);
  } catch {
    copyButton.textContent = 'SELECT & COPY';
    const selection = window.getSelection();
    selection.removeAllRanges();
    const range = document.createRange();
    range.selectNodeContents(document.querySelector('#build-command'));
    selection.addRange(range);
  }
});

document.querySelector('#year').textContent = new Date().getFullYear();
