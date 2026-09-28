// Chromium rasterization matches the browser used for the approved design proof.
const { chromium } = require(process.env.XIAOZHI_PLAYWRIGHT_MODULE || 'playwright');

(async () => {
  let input = '';
  for await (const chunk of process.stdin) input += chunk;
  const svgs = JSON.parse(input);
  const browser = await chromium.launch({ headless: true, channel: 'chrome' });
  try {
    const page = await browser.newPage({ deviceScaleFactor: 1 });
    const output = [];
    for (const svg of svgs) {
      const width = Number(svg.match(/width="(\d+)"/)[1]);
      const height = Number(svg.match(/height="(\d+)"/)[1]);
      await page.setViewportSize({ width, height });
      await page.setContent(`<html><body style="margin:0;background:transparent">${svg}</body></html>`);
      output.push((await page.screenshot({ omitBackground: true })).toString('base64'));
    }
    process.stdout.write(JSON.stringify(output));
  } finally {
    await browser.close();
  }
})().catch(error => { process.stderr.write(String(error)); process.exitCode = 1; });
