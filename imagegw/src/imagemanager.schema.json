{
    "title": "imagemanager Configuration Schema",
    "type": "object",
    "properties": {
        "WorkerThreads": {
            "description": "Number of worker threads to instantiate",
            "type": "integer",
            "minimum": 1
        },
        "DefaultLustreReplication": {
            "description": "number of copies of an image to generate",
            "type": "integer",
            "minimum": 1
        },
        "DefaultOstCount": {
            "description": "number of OSTs to strip images across (if lustre)",
            "type": "integer",
            "minimum": 1
        },
        "DefaultImageRemote": {
            "description": "Default remote location to use if unspecified",
            "type": "string"
        },
        "DefaultImageFormat": {
            "description": "Default image format",
            "type": "string"
        },
        "PullUpdateTime": {
            "description": "number of seconds an image is assumed up-to-date after pull",
            "type": "integer",
            "minimum": 1
        },
        "ImageExpirationTimeout": {
            "description": "time descriptor detailing how long until an image will expire after lookup or pull",
            "type": "string"
        }
    },
    "required": [
        "WorkerThreads", "DefaultImageRemote", "DefaultImageFormat", "PullUpdateTime",
    ]
}

