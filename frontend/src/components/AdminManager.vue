<script setup lang="ts">
/**
 * @file AdminManager.vue
 * @brief 管理员管理组件
 */
import { ref, onMounted, inject, type Ref } from 'vue'
import type { Admin, ApiResponse } from '../vite-env'

const showToast = inject<(msg: string, isError?: boolean) => void>('showToast')

const admins: Ref<Admin[]> = ref([])
const newAdminQQ: Ref<number | undefined> = ref(undefined)
const loading: Ref<boolean> = ref(false)
const saving: Ref<boolean> = ref(false)

const loadAdmins = async (): Promise<void> => {
  loading.value = true
  try {
    const resp = await fetch('/admin/api/admins')
    admins.value = await resp.json()
  } finally {
    loading.value = false
  }
}

const addAdmin = async (): Promise<void> => {
  if (!newAdminQQ.value) {
    showToast!('请输入QQ号', true)
    return
  }
  saving.value = true
  try {
    const resp = await fetch('/admin/api/admin', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ qq: newAdminQQ.value })
    })
    const data: ApiResponse = await resp.json()
    if (data.success) {
      showToast!('管理员已添加')
      newAdminQQ.value = undefined
      await loadAdmins()
    } else {
      showToast!(data.error || '添加失败', true)
    }
  } finally {
    saving.value = false
  }
}

const removeAdmin = async (qq: number): Promise<void> => {
  const resp = await fetch(`/admin/api/admin/${qq}`, { method: 'DELETE' })
  const data: ApiResponse = await resp.json()
  if (data.success) {
    showToast!('管理员已删除')
    await loadAdmins()
  }
}

onMounted(loadAdmins)
</script>

<template>
  <div>
    <div class="page-header">
      <h1 class="page-title">管理员管理</h1>
      <p class="page-subtitle">添加或移除Bot管理员</p>
    </div>

    <div class="card">
      <div class="card-header">
        <h3 class="card-title">添加管理员</h3>
      </div>
      <div class="form-group">
        <label class="form-label">QQ号</label>
        <input class="form-input" placeholder="输入QQ号" style="max-width:300px" type="number" v-model.number="newAdminQQ">
      </div>
      <button :disabled="saving" @click="addAdmin" class="btn btn-success">
        {{ saving ? '添加中...' : '添加管理员' }}
      </button>
    </div>

    <div class="card">
      <div class="card-header">
        <h3 class="card-title">管理员列表</h3>
      </div>
      <div class="table-container">
        <table v-if="!loading">
          <thead>
            <tr>
              <th>QQ号</th>
              <th style="width:100px">操作</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="admin in admins" :key="admin.qq">
              <td><code>{{ admin.qq }}</code></td>
              <td>
                <button @click="removeAdmin(admin.qq)" class="btn btn-danger btn-sm">移除</button>
              </td>
            </tr>
          </tbody>
        </table>
        <div class="empty-state" v-if="loading">
          <p>加载中...</p>
        </div>
        <div class="empty-state" v-else-if="admins.length === 0">
          <div class="empty-icon">👤</div>
          <p>暂无管理员</p>
        </div>
      </div>
    </div>
  </div>
</template>