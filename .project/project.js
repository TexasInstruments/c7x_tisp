function getComponentProperty(device)
{
    return require(`./project_${device}`).getComponentProperty();
};


module.exports = {
    getComponentProperty,
};
