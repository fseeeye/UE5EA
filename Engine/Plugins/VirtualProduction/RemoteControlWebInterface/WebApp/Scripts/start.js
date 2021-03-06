const fs = require('fs');
const path = require('path');
const cp = require('child_process');


const root = path.resolve(__dirname, '..');
const server = path.join(root, 'Server');

function execute(command, cwd, print) {
  return new Promise((resolve, reject) => {
    const child = cp.exec(command, { encoding: 'utf8', cwd });
    child.addListener('error', reject);
    child.addListener('exit', resolve);
    if (print)
      child.stdout.on('data', console.log);
  });
}

function printError(err) {
  let message = err.stdout;
  if (!message)
    message = err.message;
  if (!message)
    message = err;
  
  console.log('ERROR:', message);
}

async function build() {
  try {
    const installed = path.join(server, 'build/version.txt');
    const pkgPath = path.join(root, 'package.json');

    if (!fs.existsSync(pkgPath)) {
      console.log("ERROR: Failed to build WebApp - Can't find package.json")
      return false;
    }

    const pkg = JSON.parse(fs.readFileSync(pkgPath, 'utf8'));
    if (!pkg || !pkg.version) {
		console.log("ERROR: Failed to build WebApp - Can't parse package.json")
      return false;
    }

	const forceBuild = !!process.argv.find(arg => arg === '--build');
	if (!forceBuild && fs.existsSync(installed)) {
      const version = fs.readFileSync(installed, 'utf8');
      if (version && pkg.version === version)
        return true;
    }

    console.log('Installing dependencies');
    await execute('npm install', root, false);
	
    console.log('Building WebApp');
    await execute('npm run build', root, true);
    fs.writeFileSync(installed, pkg.version, 'utf8');
    return true;

  } catch (err) {
    printError(err);
    return false;
  }
}

async function start() {
  try {
    console.log('Starting WebApp...');
    const args = process.argv.slice(2).map(arg => `"${arg}"`).join(' ');
    const compiled = path.join(server, 'build/Server');
    await execute(`node ${compiled} ${args}`, server, true);
  
  } catch (err) {
    printError(err);
  }
}

async function buildAndStart() {
  if (await build())
    await start();
}

buildAndStart();
