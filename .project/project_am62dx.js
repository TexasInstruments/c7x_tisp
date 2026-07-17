
let path = require("path");

let device = "am62dx";

const buildOptionCombos = [
    { device: device, cpu: "c75x", cgt: "ti-c7000"},
];


function getComponentProperty() {
    let property = {};

    property.dirPath = path.resolve(__dirname, "..");
    property.type = "library"
    property.name = "TISP_C7504";
    property.isInternal = false;
    property.buildOptionCombos = buildOptionCombos;
    property.dependencies = ["AUDIOLIB_C7504"];

    return property;
}


module.exports = {
    getComponentProperty,
};
