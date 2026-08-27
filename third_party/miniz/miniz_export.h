#ifndef MINIZ_EXPORT_H
#define MINIZ_EXPORT_H

#if defined(_WIN32) || defined(_WIN64)
#ifdef MINIZ_EXPORT_BUILD
#define MINIZ_EXPORT __declspec(dllexport)
#else
#define MINIZ_EXPORT
#endif
#else
#define MINIZ_EXPORT
#endif

#endif /* MINIZ_EXPORT_H */
