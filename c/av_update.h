#ifndef PYDEV_AV_UPDATE_H
#define PYDEV_AV_UPDATE_H

typedef int (*av_database_validator)(const char *path);

int av_update_available(void);
int av_check_database_update(const char *manifest_url, const char *database_path);
int av_update_database(const char *manifest_url, const char *database_path,
                       av_database_validator validator);

#endif
