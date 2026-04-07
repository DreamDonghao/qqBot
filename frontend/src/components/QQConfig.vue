<script lang="ts" setup>
/**
 * @file QQConfig.vue
 * @brief OneBot 配置组件
 */
import {inject, ref, type Ref} from 'vue'
import type {ApiResponse, QQConfig} from '../vite-env.d'

const showToast = inject<(msg: string, isError?: boolean) => void>('showToast')
const qqConfig = inject<QQConfig>('qqConfig')
const saving: Ref<boolean> = ref(false)

const saveQQConfig = async (): Promise<void> => {
  saving.value = true
  try {
    const resp = await fetch('/admin/api/qq-config', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(qqConfig)
    })
    const data: ApiResponse = await resp.json()
    if (data.success) {
      showToast!('OneBot 配置已保存')
    } else {
      showToast!(data.error || '保存失败', true)
    }
  } finally {
    saving.value = false
  }
}
</script>

<template>
  <div>
    <div class="page-header">
      <h1 class="page-title">OneBot 配置</h1>
      <p class="page-subtitle">配置 OneBot 协议连接参数</p>
    </div>

    <div class="card">
      <div class="card-header">
        <h3 class="card-title">基础配置</h3>
      </div>
      <div class="form-row">
        <div class="form-group">
          <label class="form-label">Access Token</label>
          <input v-model="qqConfig!.accessToken" class="form-input" type="text">
          <p class="form-hint">OneBot API 访问令牌</p>
        </div>
        <div class="form-group">
          <label class="form-label">Bot QQ 号</label>
          <input v-model.number="qqConfig!.selfQQNumber" class="form-input" type="number">
          <p class="form-hint">机器人自身的 QQ 号</p>
        </div>
      </div>
      <div class="form-row">
        <div class="form-group">
          <label class="form-label">HTTP 服务地址</label>
          <input v-model="qqConfig!.qqHttpHost" class="form-input" type="text">
          <p class="form-hint">OneBot HTTP 服务地址（如 http://127.0.0.1:3000）</p>
        </div>
        <div class="form-group">
          <label class="form-label">Bot 名称</label>
          <input v-model="qqConfig!.botName" class="form-input" type="text">
          <p class="form-hint">机器人在群聊中的名称（默认：小喵）</p>
        </div>
      </div>
      <button :disabled="saving" class="btn btn-primary" @click="saveQQConfig">
        {{ saving ? '保存中...' : '保存配置' }}
      </button>
    </div>
  </div>
</template>