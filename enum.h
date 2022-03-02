#ifndef UTIL_H
#define UTIL_H

#define ENUM_MEMBER(s, ...) s,
#define ENUM(enum_name, enum_members) \
  enum enum_name { enum_members(ENUM_MEMBER) enum_name##_MAX }

#endif /* UTIL_H */
