console.log("loadelf_js_sample: QuickJS script loaded by NuttX");

const args = typeof scriptArgs !== "undefined" ? scriptArgs.slice(1) : [];
console.log(`loadelf_js_sample: argc=${args.length}`);
args.forEach((arg, index) => {
  console.log(`loadelf_js_sample: argv[${index + 1}]=${arg}`);
});

let failures = 0;

function check(name, condition) {
  console.log(`loadelf_js_sample: ${name.padEnd(24)} ${condition ? "PASS" : "FAIL"}`);
  if (!condition) {
    failures++;
  }
}

const numbers = args.map(Number).filter(Number.isFinite);
const sum = numbers.reduce((acc, value) => acc + value, 0);
const words = new Map(args.map((arg, index) => [index, arg]));
const bytes = new Uint8Array([1, 2, 3, 4]);
const object = { runtime: "QuickJS", kernel: "NuttX", sum };

check("array/reduce", sum === numbers.reduce((acc, value) => acc + value, 0));
check("map", words.size === args.length);
check("typed array", bytes.reduce((acc, value) => acc + value, 0) === 10);
check("json", JSON.parse(JSON.stringify(object)).kernel === "NuttX");

console.log(`loadelf_js_sample: sum=${sum}`);

if (failures !== 0) {
  throw new Error(`loadelf_js_sample: failures=${failures}`);
}

console.log("loadelf_js_sample: PASS");
