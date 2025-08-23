#ifndef SHADERMANAGER_H
#define SHADERMANAGER_H

#include "pre_guard.h"
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QFileSystemWatcher>
#include <QTimer>
#include <memory>
#include "post_guard.h"

class ShaderManager : public QObject, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit ShaderManager(QObject* parent = nullptr);
    ~ShaderManager();

    bool initialize();
    QOpenGLShaderProgram* getMainShaderProgram();
    bool reloadShaders();
    void cleanup();
    
    int getUniformMVP() const { return mUniformMVP; }
    int getUniformModel() const { return mUniformModel; }
    int getUniformNormalMatrix() const { return mUniformNormalMatrix; }
    
    bool isDevelopmentMode() const { return mDevelopmentMode; }

signals:
    void shadersReloaded();

private slots:
    void onShaderFileChanged(const QString& path);
    void delayedReload();

private:
    QString loadShaderSource(const QString& shaderName);
    bool createShaderProgram();
    bool detectDevelopmentMode();
    QString getEmbeddedShaderSource(const QString& shaderName);

    std::unique_ptr<QOpenGLShaderProgram> mShaderProgram;
    QFileSystemWatcher* mFileWatcher;
    QTimer* mReloadTimer;
    
    bool mDevelopmentMode;
    bool mInitialized;
    
    int mUniformMVP;
    int mUniformModel; 
    int mUniformNormalMatrix;
    
    QString mVertexShaderPath;
    QString mFragmentShaderPath;
};

#endif // SHADERMANAGER_H